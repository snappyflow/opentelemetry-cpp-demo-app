FROM sabariarunkumarp/span-demo-app-dependency:3

# Set the working directory
WORKDIR /app
COPY . .

RUN rm -rf build && mkdir build && cd build && cmake .. && cmake --build .

# Run your application
CMD ["/app/build/enroll_employee"]
