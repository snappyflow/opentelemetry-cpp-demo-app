cmake_minimum_required(VERSION 3.12)
project(enroll_employee)


set(SOURCES
    enroll_employee.cpp
)

# Add an executable target
add_executable(enroll_employee ${SOURCES})
find_package(CURL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(opentelemetry-cpp CONFIG REQUIRED)

include_directories("${OPENTELEMETRY_CPP_INCLUDE_DIRS}")

target_link_libraries(
    enroll_employee ${OPENTELEMETRY_CPP_LIBRARIES} opentelemetry_trace
     opentelemetry_common opentelemetry_http_client_curl
     opentelemetry_exporter_otlp_http opentelemetry_exporter_otlp_http_client
     opentelemetry_otlp_recordable opentelemetry_resources
     opentelemetry_metrics opentelemetry_exporter_otlp_http_metric cpprest boost_system ssl crypto)