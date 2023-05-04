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


using namespace web;
using namespace http;
using namespace http::experimental::listener;

using namespace opentelemetry::trace;

namespace trace_api      = opentelemetry::trace;
namespace trace_sdk      = opentelemetry::sdk::trace;
namespace trace_exporter = opentelemetry::exporter::trace;


// auto tracer = opentelemetry::sdk::trace::TracerProvider::GetTracerProvider()->GetTracer("employee_enrollment_manager");

sqlite3* db;

int init_db(sqlite3*& db);
int create_table(sqlite3* db);
int insert_into_table(sqlite3* db, const char *name, int age, const char *client);
json::value get_data_from_table(sqlite3* db);

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> get_tracer()
{
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer("manage-employee-client");
}

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
    // auto span = tracer->StartSpan("data-tier: inserting_employee_data_into_sqlite_db");
    auto span =  get_tracer()->StartSpan("Add_Employee_Details_To_DB");
    auto scope = get_tracer()->WithActiveSpan(span);
    span->SetAttribute(SemanticConventions::kDbName, "employeeDB");
    span->SetAttribute("db.type", "sqlite");
    span->SetAttribute("db.instance", "HDD-Optimized-v1");
    span->SetAttribute(SemanticConventions::kDbUser, "sabari");


    char insertQuery[500];
    sprintf(insertQuery, "INSERT INTO Employees (name, age, assigned_client) VALUES ('%s', %d, '%s')", name, age, client);
    span->SetAttribute(SemanticConventions::kDbStatement, insertQuery);

    int result = sqlite3_exec(db, insertQuery, nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        std::cerr << "Error inserting data: " << sqlite3_errmsg(db) << std::endl;
        // sqlite3_close(db);
        span->End();
        return result;
    }
    span->AddEvent("db insertion of employee data successful");
    span->End();
    return 0;
}
json::value get_data_from_table(sqlite3* db) {
    // auto span = tracer->StartSpan("data-tier: get_employee_data_from_sqlite_db");
    auto span =  get_tracer()->StartSpan("GET^Fetch_Employee_Data_From_DB");
    auto scope = get_tracer()->WithActiveSpan(span);

    span->SetAttribute(SemanticConventions::kDbName, "employeeDB");
    span->SetAttribute("db.type", "sqlite");
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
            // array_index_tracker = array_index_tracker + 1;
            std::cout << azColName[i] << ": " << argv[i] << std::endl;
        }
        (*responseJsonArray)[(*responseJsonArray).size()] = obj;
        return 0;
    }, &responseArray, nullptr);
    if (result != SQLITE_OK) {
        std::cerr << "Error selecting data: " << sqlite3_errmsg(db) << std::endl;
        // sqlite3_close(db);
        span->AddEvent("failed to fetch employee details from DB");
        span->End();
        return responseArray;
    }
    span->AddEvent("employee details in db are fetched successfully");
    span->End();
    return responseArray;
}
void add_employee(http_request request)
{

    auto span =  get_tracer()->StartSpan("POST^Register_Employee");
    auto scope = get_tracer()->WithActiveSpan(span);

    const http_headers& headers = request.headers();

    const utility::string_t& requestHeaderPrefix = "http.request.headers." ;
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
    }
    span->SetAttribute(SemanticConventions::kHttpMethod, "POST");
    span->SetAttribute(SemanticConventions::kHttpTarget, "/api/manage-employee");
    const utility::string_t& url = request.absolute_uri().to_string();
    const utility::string_t& clientIP = request.remote_address();
    span->SetAttribute(SemanticConventions::kHttpUrl, url);
    span->SetAttribute("net.peer.ip", clientIP);
    span->SetAttribute(SemanticConventions::kHttpScheme, "http");

    // auto span = tracer->StartSpan("app-tier: enroll_employee_with_auto_client_assignment");
    std::cout << "i am here" << std::endl;
    auto requestBody = request.extract_json().get();
    
    std::string extracted_name = requestBody[U("name")].as_string();
    const char* name = extracted_name.c_str();

    std::string extracted_assigned_client = requestBody[U("client")].as_string();
    const char* client = extracted_assigned_client.c_str();

    int age = requestBody[U("age")].as_number().to_int32();

    std::cout << name << std::endl;
    std::cout << age << std::endl;
    std::cout << client << std::endl;

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
    auto span =  get_tracer()->StartSpan("Get_Registered_Employee_Details");
    auto scope = get_tracer()->WithActiveSpan(span);
    json::value jsonResponse = get_data_from_table(db);
    span->SetAttribute(SemanticConventions::kHttpStatusCode, 200);
    span->AddEvent("failed to get registered employee details");
    const utility::string_t& requestHeaderPrefix = "http.request.headers." ;
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
    }
    span->SetAttribute(SemanticConventions::kHttpMethod, "GET");
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



int main(int argc, char* argv[]) {
    
    if (argc != 2) {
        std::cout << "Args required: ExporterEndpoint" << std::endl;
    }

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
                os_description= line.substr(12);
            }
        }
        osRelease.close();
    }
    resource::ResourceAttributes resource_attributes = {
        {"service.name", "manage-employee"}, 
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
    // auto provider = trace_sdk::TracerProviderFactory::Create(std::move(processor));

    trace_api::Provider::SetTracerProvider(provider);

   
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

    return 0;
}
