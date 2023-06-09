#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_context_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <fstream>
#include <list>

using namespace std;
namespace nostd    = opentelemetry::nostd;
namespace otlp     = opentelemetry::exporter::otlp;
namespace sdktrace = opentelemetry::sdk::trace;
namespace resource = opentelemetry::sdk::resource;
namespace trace_sdk      = opentelemetry::sdk::trace;
namespace logs_sdk  = opentelemetry::sdk::logs;
namespace logs      = opentelemetry::logs;

opentelemetry::nostd::shared_ptr<logs::Logger> get_logger()
{
  auto provider = logs::Provider::GetLoggerProvider();
  return provider->GetLogger("enroll-employee-logger", "enroll-employee");
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> get_tracer()
{
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer("enroll-employee-client");
}

typedef unsigned char uchar;
static const std::string b = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";//=
std::string base64_encode(std::string &in) {
    std::string out;

    int val=0, valb=-6;
    for (uchar c : in) {
        val = (val<<8) + c;
        valb += 8;
        while (valb>=0) {
            out.push_back(b[(val>>valb)&0x3F]);
            valb-=6;
        }
    }
    if (valb>-6) out.push_back(b[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}

namespace
{
  template <typename T>
  class HttpTextMapCarrier : public opentelemetry::context::propagation::TextMapCarrier
  {
  public:
    HttpTextMapCarrier<T>(T &headers) : headers_(headers) {}
    HttpTextMapCarrier() = default;
    virtual nostd::string_view Get(nostd::string_view key) const noexcept override
    {
      std::string key_to_compare = key.data();
      if (key == opentelemetry::trace::propagation::kTraceParent)
      {
        key_to_compare = "Traceparent";
      }
      else if (key == opentelemetry::trace::propagation::kTraceState)
      {
        key_to_compare = "Tracestate";
      }
      auto it = headers_.find(key_to_compare);
      if (it != headers_.end())
      {
        return it->second;
      }
      return "";
    }

    virtual void Set(nostd::string_view key, nostd::string_view value) noexcept override
    {
      headers_.insert(std::pair<std::string, std::string>(std::string(key), std::string(value)));
    }

    T headers_;
  };

 

  void initTracer(char *exporter_url, char *exporter_auth_user, char *exporter_auth_pass, 
                  char *sf_project_name, char *sf_app_name, char *sf_profile_id)     
  {
    otlp::OtlpHttpExporterOptions opts;
    std::string url(exporter_url);
    std::string exporter_trace_endpoint = url + "/trace";
    
    opts.url = exporter_trace_endpoint;
    opts.content_type  = otlp::HttpRequestContentType::kBinary;
    
    std::string username(exporter_auth_user);
    std::string password(exporter_auth_pass);
    std::string authString = username + ":" + password;
    std::string encodedAuthString = base64_encode(authString);

    opts.http_headers = {{"Authorization", "Basic "+encodedAuthString}};
    
    
    // opts.insecure  = true;
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
        {"service.name", "enroll-employee"}, 
        {"service.version", "1.0.1"} ,
        {"host.name", hostname},
        {"os.type", os_type},
        {"os.description", os_description},
        {"snappyflow/projectname", sf_project_name},
        {"snappyflow/appname", sf_app_name},
        {"snappyflow/profilekey", sf_profile_id}
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

    opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
        opentelemetry::nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>(
            new opentelemetry::trace::propagation::HttpTraceContext()));
  }

 

  void initLogger(char *exporter_url, char *exporter_auth_user, char *exporter_auth_pass, 
                  char *sf_project_name, char *sf_app_name, char *sf_profile_id)
  {
    otlp::OtlpHttpLogRecordExporterOptions logger_opts;
    std::string url(exporter_url);
    std::string exporter_log_endpoint = url + "/log";
    
    logger_opts.url = exporter_log_endpoint;
    logger_opts.content_type  = otlp::HttpRequestContentType::kBinary;
    std::string username(exporter_auth_user);
    std::string password(exporter_auth_pass);
    std::string authString = username + ":" + password;
    std::string encodedAuthString = base64_encode(authString);

    logger_opts.http_headers = {{"Authorization", "Basic "+encodedAuthString}};
      // opts.insecure  = true;
    resource::ResourceAttributes resource_attributes = {
        {"service.name", "enroll-employee"}, 
        {"service.version", "1.0.1"} ,
        {"snappyflow/projectname", sf_project_name},
        {"snappyflow/appname", sf_app_name},
        {"snappyflow/profilekey", sf_profile_id}
    };
    auto resource = resource::Resource::Create(resource_attributes);
    // Create OTLP exporter instance
    auto exporter  = otlp::OtlpHttpLogRecordExporterFactory::Create(logger_opts);

    logs_sdk::BatchLogRecordProcessorOptions  logProcesorOpts;
    logProcesorOpts.max_queue_size = 2048;
    logProcesorOpts.max_export_batch_size = 512;

    auto processor = logs_sdk::BatchLogRecordProcessorFactory::Create(std::move(exporter), logProcesorOpts);
    // auto processor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
    std::shared_ptr<logs::LoggerProvider> provider =
        logs_sdk::LoggerProviderFactory::Create(std::move(processor), resource);

    opentelemetry::logs::Provider::SetLoggerProvider(provider);
  }

  void CleanupTracer()
  {
    std::shared_ptr<opentelemetry::trace::TracerProvider> none;
    trace_api::Provider::SetTracerProvider(none);
  }
  void CleanupLogger()
  {
    std::shared_ptr<logs::LoggerProvider> none;
    opentelemetry::logs::Provider::SetLoggerProvider(none);
  }
}