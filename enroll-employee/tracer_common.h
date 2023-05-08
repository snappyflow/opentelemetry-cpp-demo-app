#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
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

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> get_tracer()
{
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer("enroll-employee-client");
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

  otlp::OtlpHttpExporterOptions opts;

  void initTracer(char *exporter_endpoint)
  {
    opts.url = exporter_endpoint;
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
        {"service.name", "enroll-employee"}, 
        {"service.version", "1.0.1"} ,
        {"host.name", hostname},
        {"os.type", os_type},
        {"os.description", os_description},
        {"snappyflow/projectname", "sabari-test"},
        {"snappyflow/appname", "application"},
        {"snappyflow/profilekey", "Isr7QGqj0rxnrRwgllGL1P9YrWwJVG4TzV1PK4tCrh3lh0op9AGaIsIP+scGcVSRvfOVFW4VuY0XfM0cb24RmXeQZbPUkY8rh8fb+aCMB7yTyU9tNVPvLwardUxag+/nvtxTOW43wpuunQr5EuyRrNkNwsDpQ+43m7SmzC4r2Os2vwxAONDg/jo/x9K3E3iRQBy/DweMKzqx1juSHFz5iQjsqwjnrTPzNLMj0b0ZDc/7gFtuYy7HADUD57xKTxKlw4xEA14zpbHsiTrfyPCwkLpSz4aOdwF4XuxaAoxtIZCicnfhprMnwyASWpyhtXmAgZVUn1yLVeKgNdbCAHgBG30zNkmqidwstusqptpViKg9SAp0rp6QQkoDSaJP5+d08MU8vPCrpp+HMvpmUBR/vZ2r8WzOsnW0QPCrFtlGpeq4P8ByWTMFvP4XqXtxfRca/7kV0PDPWUn/tNcDBD3L+ierjRheUUX4DF+ys5tyKaDD8UqrAMlHpqPyXYnIc6FfyWZkeWGotE5LOn5TrLEebFFj6XV1PIR15IYhptAIAxtFPMDvM3+dgzpPgjbyTdQ04bAOjgFvPh9RNiF5XMDioLh5LgABkTmVqEg9m04egiAkX6wm7Xe5c4QUDYDPQVspRCydEGt/l5Y3l72yQfmyUrqC9AN6LKa6nvZ5oDhkIzkKkU0DrwElnGG5u8i8GKxzsJTzCk+f33Hou8oWVIZgjkN1qMiH8pQ4tG0HhpuWk8PFBrhxLeojNYwkUIqLyWlQtnWEf0+WwA48G5V9u0QSue0oXkTSLiOOXo/pE+uRYU0uwSGTVB20jsXDyBADeT7V9RPoPlk2i+dyZdOwbjYTpCYeHxnrPywO4C1uNdtiX5LgMkzAEK3JLvnNkLY+qwGEnes9rv6v8UxYqP5/LJ8jLKb25LwNflEqj6YEEuYFtiA3c+mGfkEPGcvesEDRSCtl"},
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

}