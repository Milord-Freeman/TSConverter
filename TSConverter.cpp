
#include <iostream>
#include "nlohmann/json.hpp"

int main(int argc, char* argv[])
{
    for (int i = 0; i < argc; i++)
    {
        std::cout << argv[i] << "\n";
    }
}
