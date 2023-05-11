FROM sabariarunkumarp/span-demo-app-dependency:4

# Set the working directory
WORKDIR /app
COPY . .

RUN rm -rf build && mkdir build && cd build && cmake .. && cmake --build .

# Run your application
CMD ["/app/build/manage_employee"]
