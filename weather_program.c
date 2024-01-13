#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

// Struct to hold HTTP response data
struct Buffer {
    char *memory;
    size_t size;
};

// Struct to hold processed weather data
struct ProcessedData {
    char time[30];
    char location[100];
    char lastUpdated[30];
    double temperatureC;
    char condition[50];
    double windMph;
    char windDir[5];
    double pressureMb;
    int humidity;
    int cloud;
    double feelslikeC;
    double uv;
    double gustMph;
    //int isDay;
};

// Callback function for libcurl to write received data to buffer
size_t write_to_buffer(void *data, size_t size, size_t nmemb, void *pointer) {
    struct Buffer *buffer = (struct Buffer *)pointer;

    // Calculate the real size of the received data
    size_t real_size = size * nmemb;

    // Reallocate the memory for the Buffer char pointer
    buffer->memory = realloc(buffer->memory, buffer->size + real_size + 1);
    if (buffer->memory == NULL) {
        // Handle memory allocation failure
        fprintf(stderr, "Memory allocation failed.\n");
        return 0;
    }

    // Copy data to the buffer
    memcpy(buffer->memory + buffer->size, data, real_size);

    // Update the size of the buffer
    buffer->size += real_size;

    // Null-terminate the buffer
    buffer->memory[buffer->size] = '\0';

    return real_size;
}

// Function to write data to a file
int write_to_file(const char *filename, const char *data) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    if (fprintf(file, "%s", data) < 0) {
        perror("Error writing to file");
        fclose(file);
        return 1;
    }

    fclose(file);
    return 0;
}

// Function to retrieve weather data from OpenWeatherMap API
char *get_weather_data(const char *city) {
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_ALL);

    // Build the API URL
    const char *base_url = "http://api.weatherapi.com/v1/current.json?key=85858e6e8a624e51853114908230912&q=Oslo&aqi=yes";
    size_t length = strlen(base_url) + strlen(city) + 1;
    char *url = malloc(length);
    if (url == NULL) {
        // Handle memory allocation failure
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    // Format the URL
    snprintf(url, length, "%s%s", base_url, city);

    // Allocate an empty buffer that will hold the HTTP GET data
    struct Buffer buffer = { .memory = NULL, .size = 0 };

    // Initialize libcurl handle
    CURL *handle = curl_easy_init();
    if (handle == NULL) {
        // Handle libcurl initialization failure
        fprintf(stderr, "libcurl initialization failed.\n");
        free(url);
        exit(EXIT_FAILURE);
    }

    // Set curl options
    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_to_buffer);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);

    // Perform the HTTP GET request
    CURLcode result = curl_easy_perform(handle);

    // Check for HTTP request failure
    if (result != CURLE_OK) {
        fprintf(stderr, "HTTP request failed: %s\n", curl_easy_strerror(result));
        free(url);
        free(buffer.memory);
        curl_easy_cleanup(handle);
        exit(EXIT_FAILURE);
    }

    // Cleanup libcurl resources
    curl_easy_cleanup(handle);

    // Free the allocated memory for URL
    free(url);

    // Return the weather data
    return buffer.memory;
}
// Function to save JSON content to a file
void save_to_json_file(const char *filename, const char *json_content) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fprintf(file, "{\n\"parsed_data\": %s\n}", json_content);

    fclose(file);
}

// Function to parse weather data and save it to a JSON file
void parse_weather_data(const char *json_data, const char *output_filename) {
    json_object *root = json_tokener_parse(json_data);
    if (root == NULL) {
        fprintf(stderr, "JSON parsing error.\n");
        return;
    }

    if (output_filename != NULL) {
        const char *parsed_data = json_object_to_json_string(root);
        save_to_json_file(output_filename, parsed_data);
        printf("Parsed data saved to %s\n", output_filename);
    }

    json_object_put(root);
}

// Function to write processed data to a file
int write_processed_data_to_file(const char *filename, const struct ProcessedData *processed_data) {
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }
    fprintf(file, "******************************\n");
    fprintf(file, "        Weather Report        \n");
    fprintf(file, "******************************\n\n");
    
    fprintf(file, "Time: %s\n", processed_data->time);
    fprintf(file, "Location: %s\n", processed_data->location);
    fprintf(file, "Last Updated: %s\n", processed_data->lastUpdated);
    fprintf(file, "--------------------------------\n");
    fprintf(file, "Temperature: %.2f°C\n", processed_data->temperatureC);
    fprintf(file, "Condition: %s\n", processed_data->condition);
    fprintf(file, "Wind: %.2f mph, %s\n", processed_data->windMph, processed_data->windDir);
    fprintf(file, "Pressure: %.2f mb\n", processed_data->pressureMb);
    fprintf(file, "Humidity: %d%%\n", processed_data->humidity);
    fprintf(file, "Cloud Cover: %d%%\n", processed_data->cloud);
    fprintf(file, "Feels Like: %.2f°C\n", processed_data->feelslikeC);
    fprintf(file, "UV Index: %.2f\n", processed_data->uv);
    fprintf(file, "Wind Gust: %.2f mph\n", processed_data->gustMph);
    fprintf(file, "\n");

    fclose(file);
    return 0;
}

// Function to get the current time as a formatted string
void get_current_time(char *time_str, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(time_str, size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

// Function to process raw data and fill the ProcessedData struct
void process_raw_data(const char *raw_data, struct ProcessedData *processed_data) {
    json_object *root = json_tokener_parse(raw_data);

    json_object *location = json_object_object_get(root, "location");
    json_object *name = json_object_object_get(location, "name");
    json_object *country = json_object_object_get(location, "country");
    snprintf(processed_data->location, sizeof(processed_data->location), "%s, %s", json_object_get_string(name), json_object_get_string(country));

    json_object *current = json_object_object_get(root, "current");
    json_object *last_updated = json_object_object_get(current, "last_updated");
    snprintf(processed_data->lastUpdated, sizeof(processed_data->lastUpdated), "%s", json_object_get_string(last_updated));

    processed_data->temperatureC = json_object_get_double(json_object_object_get(current, "temp_c"));

    json_object *condition = json_object_object_get(current, "condition");
    json_object *condition_text = json_object_object_get(condition, "text");
    snprintf(processed_data->condition, sizeof(processed_data->condition), "%s", json_object_get_string(condition_text));

    processed_data->windMph = json_object_get_double(json_object_object_get(current, "wind_mph"));
   
    json_object *wind_dir = json_object_object_get(current, "wind_dir");
    snprintf(processed_data->windDir, sizeof(processed_data->windDir), "%s", json_object_get_string(wind_dir));

    processed_data->pressureMb = json_object_get_double(json_object_object_get(current, "pressure_mb"));
    processed_data->humidity = json_object_get_int(json_object_object_get(current, "humidity"));
    processed_data->cloud = json_object_get_int(json_object_object_get(current, "cloud"));
    processed_data->feelslikeC = json_object_get_double(json_object_object_get(current, "feelslike_c"));
    processed_data->uv = json_object_get_double(json_object_object_get(current, "uv"));
    processed_data->gustMph = json_object_get_double(json_object_object_get(current, "gust_mph"));

    get_current_time(processed_data->time, sizeof(processed_data->time));

    json_object_put(root);
}

// Function to print processed data
void print_processed_data(const struct ProcessedData *processed_data) {
    printf("Time: %s\n", processed_data->time);
    printf("Location: %s\n", processed_data->location);
    printf("Last Updated: %s\n", processed_data->lastUpdated);
    printf("Temperature: %.2f°C\n", processed_data->temperatureC);
    printf("Condition: %s\n", processed_data->condition);
    printf("Wind: %.2f mph, %s\n", processed_data->windMph, processed_data->windDir);
    printf("Pressure: %.2f mb\n", processed_data->pressureMb);
    printf("Humidity: %d%%\n", processed_data->humidity);
    printf("Cloud Cover: %d%%\n", processed_data->cloud);
    printf("Feels Like: %.2f°C\n", processed_data->feelslikeC);
    printf("UV Index: %.2f\n", processed_data->uv);
    printf("Wind Gust: %.2f mph\n", processed_data->gustMph);
    printf("\n");
}

void print_data_for_time(const char *filename, const char *user_time) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char line[256];

    // Read each line in the file
    while (fgets(line, sizeof(line), file) != NULL) {
        // Check if the line contains "Time:" and matches the user's input
        if (strstr(line, "Time:") != NULL && strstr(line, user_time) != NULL) {
            printf("%s", line); // Print the line containing the user's input time

            // Print subsequent lines until an empty line is encountered
            while (fgets(line, sizeof(line), file) != NULL && line[0] != '\n') {
                printf("%s", line);
            }

            break; // Stop reading after the desired entry
        }
    }

    fclose(file);
}

void send_email_alert(const struct ProcessedData *processed_data) {
    // Check if the temperature is below -10.0°C
    if (processed_data->temperatureC < -10.0) {
        // Build the email command with the temperature parameter
        char email_command[1024];
        snprintf(email_command, sizeof(email_command),
                 "echo -e 'From: alertweather900@gmail.com\nTo: zayynab.razao7613@gmail.com\nSubject: Urgent Weather Alert-Extereme Cold Warning\n\nDear Zaynab,\nWarning:Temperature is below threshold: %.2f°C\n  Bundle up and stay warm, cold fever expected!\n\nStay Safe,\nWeather Alert' | "
                 "curl --ssl-reqd --url 'smtps://smtp.gmail.com:465' "
                 "-u 'alertweather900@gmail.com : amcl bsph duwi qetr' "
                 "--mail-from 'alertweather900@gmail.com' "
                 "--mail-rcpt 'zayynab.razao7613@gmail.com' "
                 "--upload-file -", // Remove --mail-auth for OAuth 2.0
                 processed_data->temperatureC);

        // Execute the email command
        pid_t pid = fork();

        if (pid == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            // This is the child process
            execl("/bin/sh", "sh", "-c", email_command, NULL);
            perror("Exec failed");
            exit(EXIT_FAILURE);
        } else {
            // This is the parent process
            // Wait for the child to finish
            wait(NULL);
        }
    }
}


int main() {
    const char *city = "Oslo"; // Replace with the desired city name
    char *weather_data = get_weather_data(city);

    // Save the retrieved weather data to a file
    if (write_to_file("weather_data.txt", weather_data) == 0) {
        //printf("Weather data saved to weather_data.txt\n");
    }

    // Free the allocated memory for weather data
    free(weather_data);

    FILE *file = fopen("weather_data.txt", "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_data = malloc(file_size + 1);
    if (json_data == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return 1;
    }

    fread(json_data, 1, file_size, file);
    fclose(file);

    json_data[file_size] = '\0';

    parse_weather_data(json_data, "parsed_data.json");

    struct ProcessedData processed_data;
    get_current_time(processed_data.time, sizeof(processed_data.time));
    process_raw_data(json_data, &processed_data);
    write_processed_data_to_file("weather_forecast.txt", &processed_data);
    
    // Call send_email_alert with the processed data
    send_email_alert(&processed_data);
    
    free(json_data);

 const char *filename = "weather_forecast.txt";
    char user_time[30];
    
    printf("\n");
    printf("**************************************************************************************\n");
    printf(" -------------------------------| Weather Report |------------------------------------\n");
    printf("**************************************************************************************\n\n");
    printf("Enter time for which you want know its weather conditions: (e.g., 2024-01-06 09:54:24): ");
    fgets(user_time, sizeof(user_time), stdin);
    user_time[strcspn(user_time, "\n")] = '\0';  // Remove trailing newline

    print_data_for_time(filename, user_time);
     
    return 0;
}
