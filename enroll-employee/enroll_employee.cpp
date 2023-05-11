#include <iostream>
#include <sqlite3.h>
#include <unistd.h>
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/ext/http/client/http_client_factory.h"
#include "opentelemetry/ext/http/common/url_parser.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "tracer_common.h"

#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/uri.h>

#include <cstdlib>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <fstream>

using namespace web;
using namespace web::http::client;
using namespace http;
using namespace http::experimental::listener;

using namespace opentelemetry::trace;

namespace trace_api      = opentelemetry::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace context     = opentelemetry::context;
namespace otel_http_client = opentelemetry::ext::http::client;



utility::string_t manage_employee_endpoint;

std::string getRandomString(const std::string* stringArray, int arraySize) {
    std::mt19937 gen(std::time(0));
    std::uniform_int_distribution<> dis(0, arraySize - 1);
    int randomIndex = dis(gen);
    return stringArray[randomIndex];
}

void get_employee(http_request request)
{
    StartSpanOptions options;
    options.kind = SpanKind::kClient;
    auto span =  get_tracer()->StartSpan("GET^Get_Employee_Details",options);
    auto scope = get_tracer()->WithActiveSpan(span);
    
    

    const http_headers& headers = request.headers();
    const utility::string_t& requestHeaderPrefix = "http.request.headers." ;
    for (const auto& header : headers) {
        const utility::string_t& name = header.first;
        const utility::string_t& value = header.second;

        if (name == "User-Agent") 
        {   
            span->SetAttribute("http.user_agent", value);
            span->SetAttribute("user_agent.original", value);
        }
        else if (name == "Host") 
        {
            span->SetAttribute("http.host", value);
        }
        utility::string_t headerName =  requestHeaderPrefix + name;
        span->SetAttribute(headerName, value);
    }
    span->SetAttribute(SemanticConventions::kHttpMethod, "get");
    span->SetAttribute(SemanticConventions::kHttpTarget, "/api/enroll-employee");
    const utility::string_t& url = request.absolute_uri().to_string();
    const utility::string_t& clientIP = request.remote_address();
    span->SetAttribute(SemanticConventions::kHttpUrl, url);
    span->SetAttribute("net.peer.ip", clientIP);
    span->SetAttribute(SemanticConventions::kHttpScheme, "http");
    

    // inject current context into http header
    auto current_ctx = context::RuntimeContext::GetCurrent();
    HttpTextMapCarrier<otel_http_client::Headers> carrier;
    auto prop = context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
    prop->Inject(carrier, current_ctx);


    

  
    http_client client(manage_employee_endpoint);
    uri_builder builder(U("/api/manage-employee"));

    http_request manage_emp_request(methods::GET);
    manage_emp_request.set_request_uri(builder.to_uri());

    
    for (const auto& entry : carrier.headers_) {
        manage_emp_request.headers().add(utility::conversions::to_string_t(entry.first),
                              utility::conversions::to_string_t(entry.second));
    }

    auto logger      = get_logger();
    auto ctx         = span->GetContext();
    logger->EmitLogRecord(opentelemetry::logs::Severity::kDebug, "Remote call to external service to get employee details is initiated", ctx.trace_id(),
                        ctx.span_id(), ctx.trace_flags(),
                        opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));

    json::value jsonResponse;
    int is_successful= 0;
    client.request(manage_emp_request)
    .then([&jsonResponse, &span, &is_successful](http_response response) {
        const utility::string_t& requestHeaderPrefix = "http.response.headers." ;
        const http_headers& headers = response.headers();
        for (const auto& header : headers) {
            const utility::string_t& name = header.first;
            const utility::string_t& value = header.second;
            utility::string_t headerName =  requestHeaderPrefix + name;
            span->SetAttribute(headerName, value);
        }
        if (response.status_code() == status_codes::OK) {
            return response.extract_json();
        }
        else {
            throw std::runtime_error("HTTP request failed with status code " + std::to_string(response.status_code()));
        }
    })
    .then([&jsonResponse, &span, &is_successful](json::value localJsonResponse) {
        jsonResponse = localJsonResponse;
        is_successful = 1;
    })
    .then([](pplx::task<void> previousTask) {
        try {
            previousTask.get();
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    })
    .wait();

    if (is_successful) 
    {
        std::cout << "Response from enroll employee micro-service" << std::endl;
        logger->EmitLogRecord(opentelemetry::logs::Severity::kInfo, "successfully received employee details from remote service", ctx.trace_id(),
                        ctx.span_id(), ctx.trace_flags(),
                        opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
        span->AddEvent("successfully fetched employee details");
        span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
        span->End();
        request.reply(status_codes::OK, jsonResponse);
    }
    else
    {
        std::cout << "error observed while getting employee details; "  << std::endl;
        logger->EmitLogRecord(opentelemetry::logs::Severity::kError, "unsuccessful in receiving employee details from remote service", ctx.trace_id(),
                ctx.span_id(), ctx.trace_flags(),
                opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
        span->AddEvent("fetching employee details from upstream failed");
        span->SetAttribute(SemanticConventions::kHttpStatusCode, 500);
        span->End();
        request.reply(status_codes::InternalError, jsonResponse);
    }
}


void assign_client_and_trigger_storage_in_ledger(http_request request)
{
    StartSpanOptions options;
    options.kind = SpanKind::kClient;
    auto span =  get_tracer()->StartSpan("POST^Assign_Client_And_Register_Employee", options);
    auto scope = get_tracer()->WithActiveSpan(span);

    const utility::string_t& requestHeaderPrefix = "http.request.headers." ;
    // Iterate over the headers and print them
    const http_headers& headers = request.headers();

    for (const auto& header : headers) {
        const utility::string_t& name = header.first;
        const utility::string_t& value = header.second;

        if (name == "User-Agent") 
        {   
            span->SetAttribute("http.user_agent", value);
            span->SetAttribute("user_agent.original", value);
        }
        else if (name == "Host") 
        {
            span->SetAttribute("http.host", value);
        }
        utility::string_t headerName =  requestHeaderPrefix + name;
        span->SetAttribute(headerName, value);
        // Print the header name and value
    }
    span->SetAttribute(SemanticConventions::kHttpMethod, "post");
    span->SetAttribute(SemanticConventions::kHttpTarget, "/api/enroll-employee");
    const utility::string_t& url = request.absolute_uri().to_string();
    const utility::string_t& clientIP = request.remote_address();
    span->SetAttribute(SemanticConventions::kHttpUrl, url);
    span->SetAttribute("net.peer.ip", clientIP);
    span->SetAttribute(SemanticConventions::kHttpScheme, "http");\


    // inject current context into http header
    auto current_ctx = context::RuntimeContext::GetCurrent();
    HttpTextMapCarrier<otel_http_client::Headers> carrier;
    auto prop = context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
    prop->Inject(carrier, current_ctx);

    auto requestBody = request.extract_json().get();
    std::string extracted_name = requestBody[U("name")].as_string();
    const char* name = extracted_name.c_str();
    int age = requestBody[U("age")].as_number().to_int32();

       
    std::string listOfClients[] = {
        "cisto",
        "maplelabs",
        "google",
        "microsoft",
        "xoriant"
    };

    int arraySize = sizeof(listOfClients) / sizeof(listOfClients[0]);

    std::string randomString = getRandomString(listOfClients, arraySize);
    const char* assigned_client = randomString.c_str();
    
    http_client client(manage_employee_endpoint);
    uri_builder builder(U("/api/manage-employee"));
    
    
    
    json::value payload;
    payload = requestBody;
    payload[U("client")] = json::value::string(assigned_client);
    
    http_request add_emp_request(methods::POST);
    add_emp_request.set_body(payload);
    add_emp_request.set_request_uri(builder.to_uri());
    
    for (const auto& entry : carrier.headers_) {
        add_emp_request.headers().add(utility::conversions::to_string_t(entry.first),
                              utility::conversions::to_string_t(entry.second));
    }
    auto logger      = get_logger();
    auto ctx         = span->GetContext();
    logger->EmitLogRecord(opentelemetry::logs::Severity::kInfo, "successfully assigned client to incoming employee", ctx.trace_id(),
                        ctx.span_id(), ctx.trace_flags(),
                        opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
    logger->EmitLogRecord(opentelemetry::logs::Severity::kDebug, "initiating remote request to store employee details in DB", ctx.trace_id(),
                        ctx.span_id(), ctx.trace_flags(),
                        opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
    json::value jsonResponse;
    int is_successful= 0;
    client.request(add_emp_request)
    .then([&jsonResponse, &span, &is_successful](http_response response) {
        const utility::string_t& requestHeaderPrefix = "http.response.headers." ;
        const http_headers& headers = response.headers();
        for (const auto& header : headers) {
            const utility::string_t& name = header.first;
            const utility::string_t& value = header.second;

            utility::string_t headerName =  requestHeaderPrefix + name;
            span->SetAttribute(headerName, value);
        }
        if (response.status_code() == status_codes::OK) {
            return response.extract_json();
        }
        else {
            throw std::runtime_error("HTTP request failed with status code " + std::to_string(response.status_code()));
        }
            // std::cout << "error observed while adding employee; remote response status code" << response.status_code()  << std::endl;
            // span->SetAttribute(SemanticConventions::kHttpStatusCode, response.status_code());
            // span->AddEvent("sending employee details to upstream server failed");
            // span->End();
            // json::value jsonData;
            // request.reply(status_codes::InternalError, jsonData);
        // return response.extract_json();
    })
    .then([&jsonResponse, &span, &is_successful](json::value localJsonResponse) {
            jsonResponse = localJsonResponse;
            is_successful = 1;
    //    std::cout << "Added employee to db" << std::endl;
    //    span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
    //    span->AddEvent("successfully assigned a client to employee and details are registed in db");
    //    span->End();
    //    request.reply(status_codes::OK, jsonResponse);
    })
    .then([](pplx::task<void> previousTask) {
        try {
            previousTask.get();
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
    })
    .wait();

    if (is_successful) 
    {
        std::cout << "Added employee to db" << std::endl;
        span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
        logger->EmitLogRecord(opentelemetry::logs::Severity::kInfo, "Details are registered in DB", ctx.trace_id(),
                        ctx.span_id(), ctx.trace_flags(),
                        opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
        span->AddEvent("successfully assigned a client to employee and details are registed in db");
        span->End();
        request.reply(status_codes::OK, jsonResponse);
    }
    else
    {
        std::cout << "error observed while adding employee; "<< std::endl;
        logger->EmitLogRecord(opentelemetry::logs::Severity::kError, "communicating employee details to upstream server failed", ctx.trace_id(),
                        ctx.span_id(), ctx.trace_flags(),
                        opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
        span->SetAttribute(SemanticConventions::kHttpStatusCode, 500);
        span->AddEvent("sending employee details to upstream server failed");
        span->End();
        request.reply(status_codes::InternalError, jsonResponse);
    }

}

namespace otlp = opentelemetry::exporter::otlp;
namespace resource  = opentelemetry::sdk::resource;


int main() {

    char* forwarder_url = std::getenv("SF_FORWARDER_URL");
    if (forwarder_url == nullptr) {
        std::cerr << "SF_FORWARDER_URL is not set; exiting;" << std::endl;
        return -1;
    }
    
    char* forwarder_auth_user = std::getenv("SF_FORWARDER_AUTH_USER");
    if (forwarder_auth_user == nullptr) {
        std::cerr << "SF_FORWARDER_AUTH_USER is not set; exiting;" << std::endl;
        return -1;
    }

    char* forwarder_auth_pass = std::getenv("SF_FORWARDER_AUTH_PASS");
    if (forwarder_auth_pass == nullptr) {
        std::cerr << "SF_FORWARDER_AUTH_PASS is not set; exiting;" << std::endl;
        return -1;
    }
    
    char* sf_target_project_name = std::getenv("SF_TARGET_PROJECT_NAME");
    if (sf_target_project_name == nullptr) {
        std::cerr << "SF_TARGET_PROJECT_NAME is not set; exiting;" << std::endl;
        return -1;
    }

    char* sf_target_app_name = std::getenv("SF_TARGET_APP_NAME");
    if (sf_target_app_name == nullptr) {
        std::cerr << "SF_TARGET_APP_NAME is not set; exiting;" << std::endl;
        return -1;
    }

    char* sf_target_profile_key = std::getenv("SF_TARGET_PROFILE_KEY");
    if (sf_target_profile_key == nullptr) {
        std::cerr << "SF_TARGET_PROFILE_KEY is not set; exiting;" << std::endl;
        return -1;
    }

    char* manager_employee_service_endpoint = std::getenv("MANAGER_EMPLOYEE_ENDPOINT");
    if (forwarder_url == nullptr) {
        std::cerr << "MANAGER_EMPLOYEE_ENDPOINT is not set; exiting;" << std::endl;
        return -1;
    }

    manage_employee_endpoint = utility::conversions::to_string_t(manager_employee_service_endpoint);
  
    initTracer(forwarder_url, forwarder_auth_user, forwarder_auth_pass, 
                sf_target_project_name, sf_target_app_name, sf_target_profile_key);
    initLogger(forwarder_url, forwarder_auth_user, forwarder_auth_pass, 
                sf_target_project_name, sf_target_app_name, sf_target_profile_key);

    http_listener listener(U("http://0.0.0.0:9090/api/enroll-employee"));
    listener.support(methods::POST, assign_client_and_trigger_storage_in_ledger);
    listener.support(methods::GET, get_employee);

    try
    {
        listener.open().wait();
        std::cout << "Listening on http://0.0.0.0:9090/api/enroll-employee" << std::endl;

        while (true);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    CleanupTracer();
    CleanupLogger();
    return 0;
}
