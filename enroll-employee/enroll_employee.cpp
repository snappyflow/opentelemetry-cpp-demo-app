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
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/semantic_conventions.h"

#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/uri.h>


#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <fstream>

using namespace web;
using namespace web::http::client;
using namespace http;
using namespace http::experimental::listener;

using namespace opentelemetry::trace;

namespace trace_api      = opentelemetry::trace;
namespace trace_sdk      = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;




utility::string_t manage_employee_endpoint;
opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> get_tracer()
{
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer("enroll-employee-client");
}

std::string getRandomString(const std::string* stringArray, int arraySize) {
    std::mt19937 gen(std::time(0));
    
    std::uniform_int_distribution<> dis(0, arraySize - 1);
    int randomIndex = dis(gen);

    return stringArray[randomIndex];
}

void get_employee(http_request request)
{

    auto span =  get_tracer()->StartSpan("GET^Get_Employee_Details");
    auto scope = get_tracer()->WithActiveSpan(span);

    const http_headers& headers = request.headers();

    // const utility::string_t& requestHeaderPrefix = utility::conversions::to_string_t("http.request.headers.");
    const utility::string_t& requestHeaderPrefix = "http.request.headers." ;
    // Iterate over the headers and print them
    for (const auto& header : headers) {
        const utility::string_t& name = header.first;
        const utility::string_t& value = header.second;

        if (name == "User-Agent") 
        {   
            span->SetAttribute("http.user_agent", value);
            span->SetAttribute(SemanticConventions::kUserAgentOriginal, value);
        }
        else if (name == "Host") 
        {
            span->SetAttribute("http.host", value);
        }
        utility::string_t headerName =  requestHeaderPrefix + name;
        span->SetAttribute(headerName, value);
        // Print the header name and value
    }
    span->SetAttribute(SemanticConventions::kHttpMethod, "GET");
    span->SetAttribute(SemanticConventions::kHttpTarget, "/api/enroll-employee");
    const utility::string_t& url = request.absolute_uri().to_string();
    const utility::string_t& clientIP = request.remote_address();
    span->SetAttribute(SemanticConventions::kHttpUrl, url);
    span->SetAttribute("net.peer.ip", clientIP);
    span->SetAttribute(SemanticConventions::kHttpScheme, "http");
    
    http_client client(manage_employee_endpoint);
    uri_builder builder(U("/api/manage-employee"));

    http_request manage_emp_request(methods::GET);
    manage_emp_request.set_request_uri(builder.to_uri());




    client.request(manage_emp_request)
    .then([&request,&span](http_response response) {
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
            std::cout << "error observed while getting employee details; remote response status code" << response.status_code() << std::endl;
            span->AddEvent("fetching employee details from upstream failed");
            span->SetAttribute(SemanticConventions::kHttpStatusCode, response.status_code());
            span->End();
            json::value jsonData;
            request.reply(status_codes::InternalError, jsonData);
        }
        // return response.extract_json();
    })
    .then([&request, &span](json::value jsonResponse) {
    
        std::cout << "Response from enroll employee micro-service" << std::endl;
        std::cout <<  jsonResponse << std::endl;
        span->AddEvent("successfully fetched employee details");
        span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
        span->End();
       request.reply(status_codes::OK, jsonResponse);
    })
    .wait();
    // To-Do dummy response
    span->AddEvent("unkown error while fetching employee details");
    span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
    span->End();

    json::value dummyData;
    request.reply(status_codes::InternalError, dummyData);

}


void assign_client_and_trigger_storage_in_ledger(http_request request)
{
    auto span =  get_tracer()->StartSpan("POST^Assign_Client_And_Register_Employee");
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
            span->SetAttribute(SemanticConventions::kUserAgentOriginal, value);
        }
        else if (name == "Host") 
        {
            span->SetAttribute("http.host", value);
        }
        utility::string_t headerName =  requestHeaderPrefix + name;
        span->SetAttribute(headerName, value);
        // Print the header name and value
    }
    span->SetAttribute(SemanticConventions::kHttpMethod, "POST");
    span->SetAttribute(SemanticConventions::kHttpTarget, "/api/enroll-employee");
    const utility::string_t& url = request.absolute_uri().to_string();
    const utility::string_t& clientIP = request.remote_address();
    span->SetAttribute(SemanticConventions::kHttpUrl, url);
    span->SetAttribute("net.peer.ip", clientIP);
    span->SetAttribute(SemanticConventions::kHttpScheme, "http");\


    auto requestBody = request.extract_json().get();
    std::string extracted_name = requestBody[U("name")].as_string();
    const char* name = extracted_name.c_str();
    int age = requestBody[U("age")].as_number().to_int32();

    std::cout << name << std::endl;
    std::cout << age << std::endl;
    
    std::string listOfClients[] = {
        "cisto",
        "maplelabse",
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

    client.request(add_emp_request)
    .then([&request, &span](http_response response) {
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
            std::cout << "error observed while adding employee; remote response status code" << response.status_code()  << std::endl;
            span->SetAttribute(SemanticConventions::kHttpStatusCode, response.status_code());
            span->AddEvent("sending employee details to upstream server failed");
            span->End();
            json::value jsonData;
            request.reply(status_codes::InternalError, jsonData);
        }
        // return response.extract_json();
    })
    .then([&request, &span](json::value jsonResponse) {
       std::cout << "Added employee to db" << std::endl;
       span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
       span->AddEvent("successfully assigned a client to employee and details are registed in db");
       span->End();
       request.reply(status_codes::OK, jsonResponse);
    })
    .wait();

    span->AddEvent("unknown error while processing employee details");

    span->SetAttribute(SemanticConventions::kHttpStatusCode, 500);
    span->End();
    // To-Do dummy response
    json::value dummyData;
    request.reply(status_codes::InternalError, dummyData);
}

namespace otlp = opentelemetry::exporter::otlp;
namespace resource  = opentelemetry::sdk::resource;


int main(int argc, char* argv[]) {

    if (argc != 3) {
        std::cout << "Args required: ExporterEndpoint, manage_employee_endpoint " << std::endl;
    }
    manage_employee_endpoint = utility::conversions::to_string_t(argv[2]);
    
    
    
    
    otlp::OtlpHttpExporterOptions opts;
    opts.url = argv[1];
    opts.content_type  = otlp::HttpRequestContentType::kBinary;
      
    

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
      
    std::ifstream osRelease("/etc/os-release");
    std::string line;
    std::string os_type;
    std::string os_description;
    if (osRelease.is_open()) {
        while (getline(osRelease, line)) {
            if (line.substr(0, 8) == "ID_LIKE=") 
            {
                os_type = line.substr(8);
            } 
            else if (line.substr(0, 12) == "PRETTY_NAME=") 
            {
                os_description= line.substr(8);
            }
        }
        osRelease.close();
    }
    resource::ResourceAttributes resource_attributes = {
        {"service.name", "enroll-employee"}, 
        {"service.version", "1.0.1"} ,
        {"host.name", hostname},
        {"os.type", os_type},
        {"os.description", os_description}
    };
    auto resource = resource::Resource::Create(resource_attributes);

    auto exporter = otlp::OtlpHttpExporterFactory::Create(opts);
    trace_sdk::BatchSpanProcessorOptions batchSpanOpts;
    batchSpanOpts.max_queue_size = 2048;
    batchSpanOpts.max_export_batch_size = 512;
    auto processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), batchSpanOpts);

    std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
        trace_sdk::TracerProviderFactory::Create(std::move(processor), resource);

    trace_api::Provider::SetTracerProvider(provider);
  

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

    return 0;
}
