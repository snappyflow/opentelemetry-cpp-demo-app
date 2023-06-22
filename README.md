# Opentelemetry-CPP-Demo-App

The project aims to provide a sample c++ application in docker, instrumented with opentelemetry  trace and log provider. Collected traces/logs are exported to custom endpoints.



The application facilitates 2 micro-services interacting with each-other. Detailed interaction is explained in [opentelemetry-cpp-demo-app/Design.jpg at master · snappyflow/opentelemetry-cpp-demo-app (github.com)](https://github.com/snappyflow/opentelemetry-cpp-demo-app/blob/master/Design.jpg)





## Building Microservices



BaseDependencyDockerFile installs common packages for both the micro-services.  Individual microservice's needs to inherit the pre-built container based out of this BaseDependencyDockerFile respectively in their build environment [on respective microservice's DockerFile], and can be built



## Deploying Microservice



Once microservice's are built, the *runtime-env-file.env* file needs to configured for **MANAGER_EMPLOYEE_ENDPOINT**. The ip-address is the variable config. 

Now both the micro-services can be deployed in same machine/docker environment. Docker execution command for individual service, is provider in respective *runtime-instructions.txt* file.

For SnappyFlow Integration, destiny forwarder's configuration and targets needs to be configured.



## 

## API's Considered For Testing



(1) curl --location --request GET 'http://<-enroll-employee-service-ip>:9090/api/enroll-employee/'

(2) curl --location --request POST 'http://<-enroll-employee-service-ip>:9090/api/enroll-employee/' \
        --header 'Content-Type: application/json' \
        --data-raw '{
            "name": "rohit",
            "age": 20
        }'



Above api's returns a desired state of 200 response. To get 5xx  response, the manager-employee-service can be halted/stopped, and the api's can be executed











