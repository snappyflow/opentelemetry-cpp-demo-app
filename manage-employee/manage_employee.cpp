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


using namespace web;
using namespace http;
using namespace http::experimental::listener;

using namespace opentelemetry::trace;

namespace trace_api      = opentelemetry::trace;
namespace trace_sdk      = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;
namespace context     = opentelemetry::context;
namespace otel_http_client = opentelemetry::ext::http::client;

// auto tracer = opentelemetry::sdk::trace::TracerProvider::GetTracerProvider()->GetTracer("employee_enrollment_manager");

sqlite3* db;

int init_db(sqlite3*& db);
int create_table(sqlite3* db);
int insert_into_table(sqlite3* db, const char *name, int age, const char *client);
json::value get_data_from_table(sqlite3* db);


std::string getRandomString(const std::string* stringArray, int arraySize) {
    std::mt19937 gen(std::time(0));
    
    std::uniform_int_distribution<> dis(0, arraySize - 1);
    int randomIndex = dis(gen);

    return stringArray[randomIndex];
}


int init_db(sqlite3*& db) {
    int result = sqlite3_open("employee_store.db", &db);
    if (result != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        return result;
    }
    return 0;
}
int create_table(sqlite3* db) {
    const char* createTableQuery = "CREATE TABLE IF NOT EXISTS Employees (id INTEGER PRIMARY KEY, name TEXT, age INTEGER, assigned_client TEXT);";
    int result = sqlite3_exec(db, createTableQuery, nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating table: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return result;
    }
    return 0;

}
int insert_into_table(sqlite3* db, const char *name, int age, const char *client) {

    StartSpanOptions options;
    options.kind = SpanKind::kInternal;
    auto span =  get_tracer()->StartSpan("Add_Employee_Details_To_DB", options);
    auto scope = get_tracer()->WithActiveSpan(span);
    
    auto logger      = get_logger();
    auto ctx         = span->GetContext();


    span->SetAttribute(SemanticConventions::kDbName, "employeeDB");
    span->SetAttribute("db.type", "sqlite");
    span->SetAttribute("db.action", "query");
    span->SetAttribute("db.instance", "HDD-Optimized-v1");
    span->SetAttribute(SemanticConventions::kDbUser, "sabari");


    char insertQuery[500];
    sprintf(insertQuery, "INSERT INTO Employees (name, age, assigned_client) VALUES ('%s', %d, '%s')", name, age, client);
    
    span->SetAttribute(SemanticConventions::kDbStatement, insertQuery);
    int result = sqlite3_exec(db, insertQuery, nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        std::cerr << "Error inserting data: " << sqlite3_errmsg(db) << std::endl;
        logger->EmitLogRecord(opentelemetry::logs::Severity::kError, "Insertion of employee data into DB is unsuccessful", ctx.trace_id(), ctx.span_id(), ctx.trace_flags(),
                opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
        span->End();
        return result;
    }
    span->AddEvent("db insertion of employee data successful");
    logger->EmitLogRecord(opentelemetry::logs::Severity::kInfo, "Insertion of employee data into DB is successful", ctx.trace_id(), ctx.span_id(), ctx.trace_flags(),
                    opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
    span->End();
    return 0;
}

json::value get_data_from_table(sqlite3* db) {

    StartSpanOptions options;
    options.kind = SpanKind::kInternal;
    auto span =  get_tracer()->StartSpan("GET^Fetch_Employee_Data_From_DB", options);
    auto scope = get_tracer()->WithActiveSpan(span);

    auto logger      = get_logger();
    auto ctx         = span->GetContext();


    span->SetAttribute(SemanticConventions::kDbName, "employeeDB");
    span->SetAttribute("db.type", "sqlite");
    span->SetAttribute("db.action", "query");
    span->SetAttribute("db.instance", "HDD-Optimized-v1");
    span->SetAttribute(SemanticConventions::kDbUser, "sabari");


    json::value responseArray;
    const char* selectDataQuery = "SELECT id, name, age, assigned_client FROM Employees;";
    span->SetAttribute(SemanticConventions::kDbStatement, selectDataQuery);

    int result = sqlite3_exec(db, selectDataQuery, [](void* responseArray, int argc, char** argv, char** azColName) -> int {
        json::value *responseJsonArray = static_cast<json::value*>(responseArray);
        json::value obj;
        for (int i = 0; i < argc; i++) {   
            (obj)[U(azColName[i])] = json::value::string(U(argv[i]));
        }
        (*responseJsonArray)[(*responseJsonArray).size()] = obj;
        return 0;
    }, &responseArray, nullptr);
    if (result != SQLITE_OK) {
        std::cerr << "Error selecting data: " << sqlite3_errmsg(db) << std::endl;
        // sqlite3_close(db);
        span->AddEvent("failed to fetch employee details from DB");
        logger->EmitLogRecord(opentelemetry::logs::Severity::kError, "failed to fetch employee details from DB", ctx.trace_id(), ctx.span_id(), ctx.trace_flags(),
                    opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
        span->End();
        return responseArray;
    }
    logger->EmitLogRecord(opentelemetry::logs::Severity::kInfo, "employee details in db are fetched successfully", ctx.trace_id(), ctx.span_id(), ctx.trace_flags(),
                    opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));
    span->AddEvent("employee details in db are fetched successfully");
    span->End();
    return responseArray;
}
void add_employee(http_request request)
{

    StartSpanOptions options;
    options.kind = SpanKind::kServer;
    
    if (request.headers().has("traceparent")) {
        HttpTextMapCarrier<otel_http_client::Headers> carrier;
        const std::string& traceparentVal = request.headers()["traceparent"];
        carrier.Set("traceparent", traceparentVal);
         if (request.headers().has("tracestate")) {
            const std::string& tracestateVal = request.headers()["tracestate"];
            carrier.Set("tracestate", tracestateVal);
        }
        auto prop        = context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
        auto current_ctx = context::RuntimeContext::GetCurrent();
        auto new_context = prop->Extract(carrier, current_ctx);
        options.parent   = GetSpan(new_context)->GetContext();
    }

    auto span =  get_tracer()->StartSpan("POST^Register_Employee", options);
    auto scope = get_tracer()->WithActiveSpan(span);

    auto logger      = get_logger();
    auto ctx         = span->GetContext();
    logger->EmitLogRecord(opentelemetry::logs::Severity::kDebug, "employee details DB insert request successfully received", ctx.trace_id(), ctx.span_id(), ctx.trace_flags(),
                    opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));

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
    span->SetAttribute(SemanticConventions::kHttpMethod, "post");
    span->SetAttribute(SemanticConventions::kHttpTarget, "/api/manage-employee");

    const utility::string_t& url = request.absolute_uri().to_string();
    const utility::string_t& clientIP = request.remote_address();

    span->SetAttribute(SemanticConventions::kHttpUrl, url);
    span->SetAttribute("net.peer.ip", clientIP);
    span->SetAttribute(SemanticConventions::kHttpScheme, "http");

    auto requestBody = request.extract_json().get();
    
    std::string extracted_name = requestBody[U("name")].as_string();
    const char* name = extracted_name.c_str();

    std::string extracted_assigned_client = requestBody[U("client")].as_string();
    const char* client = extracted_assigned_client.c_str();

    int age = requestBody[U("age")].as_number().to_int32();


    
    json::value jsonResponse;
    int status = insert_into_table(db, name, age, client);
    if (!status)
    {
       jsonResponse = get_data_from_table(db);
    } 
    else 
    {
        span->SetAttribute(SemanticConventions::kHttpStatusCode, 500);
        span->End();
        request.reply(status_codes::InternalError, jsonResponse);
    }
 
    span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
    span->End();
    request.reply(status_codes::OK, jsonResponse);
}

void get_employee(http_request request)
{
    StartSpanOptions options;
    options.kind = SpanKind::kServer;
    if (request.headers().has("traceparent")) {
        HttpTextMapCarrier<otel_http_client::Headers> carrier;
        const std::string& traceparentVal = request.headers()["traceparent"];
        carrier.Set("traceparent", traceparentVal);
         if (request.headers().has("tracestate")) {
            const std::string& tracestateVal = request.headers()["tracestate"];
            carrier.Set("tracestate", tracestateVal);
        }
        auto prop        = context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
        auto current_ctx = context::RuntimeContext::GetCurrent();
        auto new_context = prop->Extract(carrier, current_ctx);
        options.parent   = GetSpan(new_context)->GetContext();

    }
   
    auto span =  get_tracer()->StartSpan("Get_Registered_Employee_Details", options);
    auto scope = get_tracer()->WithActiveSpan(span);

    auto logger      = get_logger();
    auto ctx         = span->GetContext();
    logger->EmitLogRecord(opentelemetry::logs::Severity::kDebug, "employee details fetch request successfully received", ctx.trace_id(), ctx.span_id(), ctx.trace_flags(),
                    opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now()));

    const http_headers& headers = request.headers();
   
    json::value jsonResponse = get_data_from_table(db);
    span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
    span->AddEvent("failed to get registered employee details");
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
    span->SetAttribute(SemanticConventions::kHttpTarget, "/api/manage-employee");
    const utility::string_t& url = request.absolute_uri().to_string();
    const utility::string_t& clientIP = request.remote_address();
    span->SetAttribute(SemanticConventions::kHttpUrl, url);
    span->SetAttribute("net.peer.ip", clientIP);
    span->SetAttribute(SemanticConventions::kHttpScheme, "http");
    span->End();
    request.reply(status_codes::OK, jsonResponse);
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


    initTracer(forwarder_url, forwarder_auth_user, forwarder_auth_pass, 
                sf_target_project_name, sf_target_app_name, sf_target_profile_key);
   
    init_db(db);
    create_table(db);

    http_listener listener(U("http://0.0.0.0:9091/api/manage-employee"));
    listener.support(methods::POST, add_employee);
    listener.support(methods::GET, get_employee);

    try
    {
        listener.open().wait();
        std::cout << "Listening on http://0.0.0.0:9091/api/manage-employee" << std::endl;

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
