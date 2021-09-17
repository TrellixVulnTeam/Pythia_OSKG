#ifdef _WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#else
#define __stdcall
#define _strdup strdup
#define sprintf_s sprintf
#endif

#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include "../Pythia/SQFGenerator.h"
#include "../Pythia/SQFGenerator.cpp"

#pragma warning( disable : 4996 ) // Disable warning message 4996 (sscanf).

#define ARMA_EXTENSION_BUFFER_SIZE (10*1024)

typedef void (__stdcall *RVExtension_t)(char *output, int outputSize, const char *function);

RVExtension_t RVExtension;

void RVExtensionCheck(char *output, int outputSize, const char *function)
{
    output[outputSize - 1] = '\0';
    RVExtension(output, outputSize, function);
    if (output[outputSize - 1] != '\0')
    {
        std::cout << "BUFFER OVERFLOW!!!" << std::endl;
        std::cout << "Press enter to continue...";
        std::cin.get();
        exit(1);
    }
}

void test()
{
    char output[ARMA_EXTENSION_BUFFER_SIZE];
    const int iterations = 10000;
    const char *command = "[\"pythia.ping\", \"asd\", \"ert\", 3]";
    //const char *command = "[\"pythia.test\"]";

    std::cout << "Calling " << iterations << " times: " << command << std::endl;

    // First call to initialize everything (in case it is needed)
    RVExtensionCheck(output, sizeof(output), command);

    auto start = std::chrono::system_clock::now();
    for (int i = iterations; i > 0; i--)
    {
        RVExtensionCheck(output, sizeof(output), command);
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed = end - start;

    std::cout << "Last call output: " << output << std::endl;
    std::cout << "Each function time: " << elapsed.count() / 10000.0 / (double)iterations << "ms" << std::endl;
}

std::string createPingRequest(std::string sqf)
{
    return std::string("['pythia.ping', ") + sqf.c_str() + "]";
}

void parseMultipart(const char *output, int &id, int &count)
{
    sscanf(output, "[\"m\",%d,%d]", &id, &count);
}

int compareRegularResponse(const char *response, std::string &expected)
{
    const char responseTemplate[] = "[\"r\",";
    const int payloadOffset = sizeof(responseTemplate) - 1;

    if (strncmp(response, responseTemplate, payloadOffset) == 0)
    {
        if (!strncmp(response + payloadOffset, expected.c_str(), expected.size()))
        {
            if (!strcmp(response + payloadOffset + expected.size(), "]"))
            {
                return 0;
            }
        }

        std::cout << "Expected: " << responseTemplate << expected << "]" << std::endl;
        std::cout << "Got:      " << response << std::endl;
        return 1;
    }

    return -1; // Not a regular response
}

std::string handleMultipart(int id, int count)
{
    char output[ARMA_EXTENSION_BUFFER_SIZE];
    std::string multipartRequest = std::string("['pythia.multipart',") + std::to_string(id) + ']';
    std::string fullOutput;

    for (int i = 0; i < count; i++)
    {
        output[0] = '\0';
        RVExtensionCheck(output, sizeof(output), multipartRequest.c_str());
        fullOutput += output;
    }
    //std::cout << fullOutput << std::endl;
    return fullOutput;
}

int test_fuzzing_single()
{
    char output[ARMA_EXTENSION_BUFFER_SIZE];

    const char multipartTemplate[] = "[\"m\",";
    const int payloadOffset = sizeof(multipartTemplate) - 1;

    SQFGenerator builder = SQFGenerator(0, 100);
    std::string sqf = builder.generate(2);
    std::string request = createPingRequest(sqf);

    RVExtensionCheck(output, sizeof(output), request.c_str());

    // Check for regular response
    int regularResponse = compareRegularResponse(output, sqf);
    if (regularResponse != -1)
        return regularResponse;

    if (strncmp(output, multipartTemplate, payloadOffset) == 0)
    {
        //std::cout << output << std::endl;
        int id, count;
        parseMultipart(output, id, count);
        std::string multipartOutput = handleMultipart(id, count);

        int multipartResponse = compareRegularResponse(multipartOutput.c_str(), sqf);
        if (multipartResponse == -1)
        {
            std::cout << "Got unknown response: " << multipartOutput << std::endl;
        }
        return multipartResponse;
    }
    else
    {
        std::cout << "Got unknown response: " << output << std::endl;
        return 1;
    }
}

void test_fuzzing_multiple()
{
    int iterations = 10000;

    auto start = std::chrono::system_clock::now();
    for (int i = 0; i < iterations; i++)
    {
        if (i % 10 == 0)
            std::cout << "Test: " << std::to_string(i) << std::endl;

        if (test_fuzzing_single() != 0)
            return;
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed = end - start;
    std::cout << "Tests OK!" << std::endl;
    std::cout << "Each function time: " << elapsed.count() / 10000.0 / (double)iterations << "ms" << std::endl;
}

void test_coroutines()
{
    char output[ARMA_EXTENSION_BUFFER_SIZE];
    const int iterations = 10000;
    const char *command = "['python.coroutines.test_coroutines']";
    char *response = _strdup("['pythia.continue',         , 'tralala something']");
    char number[10];

    int continue_val;

    std::cout << "Calling " << iterations << " times: " << command << std::endl;

    // First call to initialize everything (in case it is needed)
    RVExtensionCheck(output, sizeof(output), command);

    auto start = std::chrono::system_clock::now();
    for (int i = iterations; i > 0; i--)
    {
        RVExtensionCheck(output, sizeof(output), command);
        while (output[2] == 's')
        {
            continue_val = atoi(output + 5);
            sprintf_s(number, "%6d", continue_val);
            for (int j = 0; j < 6; j++)
            {
                response[20 + j] = number[j];
            }
            RVExtensionCheck(output, sizeof(output), response);
        }
    }
    auto end = std::chrono::system_clock::now();
    auto elapsed = end - start;

    std::cout << "Last call output: " << output << std::endl;
    std::cout << "Each function time: " << elapsed.count() / 10000.0 / (double)iterations << "ms" << std::endl;
}

#ifdef _WIN64
#define PYTHIA_DLL "Pythia_x64.dll"
#define PYTHON_SET_PATH_DLL "PythiaSetPythonPath_x64.dll"
#define FUNCNAME "RVExtension"
#else
#define PYTHIA_DLL "Pythia.dll"
#define PYTHON_SET_PATH_DLL "PythiaSetPythonPath.dll"
#define FUNCNAME "_RVExtension@12"
#endif


int main()
{
    HINSTANCE hInstSetPath = LoadLibrary(TEXT(PYTHON_SET_PATH_DLL));
    if (!hInstSetPath)
    {
        std::cout << "Could not open " << PYTHON_SET_PATH_DLL << std::endl;
    }

    HINSTANCE hInstLibrary = LoadLibrary(TEXT(PYTHIA_DLL));

    if (hInstLibrary)
    {
        RVExtension = (RVExtension_t)GetProcAddress(hInstLibrary, FUNCNAME);

        if (RVExtension)
        {
            //test();
            test_fuzzing_multiple();
        }
        else
        {
            std::cout << "Could not get RVExtension function." << std::endl;
        }
        FreeLibrary(hInstLibrary);
    }
    else
    {
        std::cout << "Could not open library dll." << std::endl;
    }

    std::cout << "Press enter to continue...";
    std::cin.get();
    return 0;
}

