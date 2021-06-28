
#include <iostream>
#include <iostream> // For stringstream
#include <iomanip> // For setw() 
#include <cctype> // for toupper() and tolower()
#include <fstream> // For ifstream and ofstream
#include <functional> // For std::function
#include <algorithm> // For transform
#include <set>
#include "nlohmann/json.hpp"

int main(int argc, char* argv[])
{
	// TODO: add try/catch block for bad input
	const std::string pathToJsonFile = argv[1];
	const std::string pathToTypeScriptFile = argv[2];

	std::ifstream input(pathToJsonFile);
	std::ofstream output(pathToTypeScriptFile);

	enum class mode
	{
		module = 0,
		interface
	};
	const mode currentMode = mode::module; // 'interface' | 'module'
	// TODO: Implement interface mode correctly

	const auto getPrefix = [&currentMode]() -> std::string {return currentMode == mode::interface ? "export interface SkyrimPlatform { \n" : ""; };
	const auto getPostfix = [&currentMode]() -> std::string {return currentMode == mode::interface ? "}\n" : ""; };

	nlohmann::json j;
	input >> j;

	std::string tab = "    ";

	const std::set<std::string> ignored = { "TESModPlatform.Add", "Math", "MpClientPlugin" };
	std::set<std::string> dumped = {};
	const std::map<std::string, std::string> functionNameOverrides = { {"getplayer","getPlayer"}};

	auto prettify = [&](std::string name, std::function<int(int)> func = toupper) -> std::string
	{
		char firstChar = func(name.at(0));
		name.erase(name.begin());

		bool isDifference = false;
		for (int i = 0; i < name.length(); i++)
		{
			if (toupper(name.at(i)) != name.at(i) || tolower(name.at(i)) != name.at(i))
			{
				isDifference = true;
				break;
			}
		}

		if (isDifference)
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });

		return firstChar + name;
	};

	auto parseReturnValue = [&](std::string rawType, std::string objectTypeName) -> std::string
	{
		// Fuck C++ Strings types 
		if (rawType == "Int" || rawType == "Float") return "number";
		if (rawType == "Bool")						return "boolean";
		if (rawType == "String")					return "string";
		if (rawType == "IntArray" || "FloatArray")  return "number[] | null";
		if (rawType == "BoolArray")					return "boolean[] | null";
		if (rawType == "StringArray")				return "string[] | null";
		if (rawType == "None")						return "void";
		if (rawType == "Object")					return (!objectTypeName.empty() ? prettify(objectTypeName) : "Form") + " | null";
		if (rawType == "ObjectArray")				return "PapyrusObject[] | null";
		//TODO: fix return value with bat rawType
	};

	auto dumpFunction = [&](std::string className, nlohmann::json f, bool isGlobal)
	{
		if (ignored.contains((className + "." + f["name"].get<std::string>()).c_str()))
		{
			return;
		}

		auto funcName =
			functionNameOverrides.contains(f["name"].get<std::string>())
			? functionNameOverrides.at(f["name"].get<std::string>())
			: f["name"].get<std::string>();

		output << tab << (isGlobal ? "static " : "") << prettify(funcName, ::tolower);
		// It is no longer important for us in what case the letters were, but then it is more convenient to work with them in lower case 
		std::transform(funcName.begin(), funcName.end(), funcName.begin(), [](unsigned char c) { return std::tolower(c); });

		output << "(";
		bool isAddOrRemove = (funcName == "additem" || funcName == "removeitem");

		for (int i = 0; auto & argument : f["arguments"]) {
			bool isSetMotioTypeFistArg = funcName == "setmotiontype" && i;
			i++;
			std::string argType = isSetMotioTypeFistArg ? "MotionType" : parseReturnValue(argument["type"]["rawType"], argument["type"]["objectTypeName"]);
			auto argumentName = (argument["name"] == "in") ? "_in" : argument["name"];
			output << argumentName << ": " << argType;
			if (i != f["arguments"].size() - 1)
			{
				output << ", ";
			}
		}

		auto returnType = parseReturnValue(f["returnType"]["rawType"].get<std::string>(), f["returnType"]["objectTypeName"]);

		if (f["isLatent"].get<bool>())
		{
			if (!isAddOrRemove)
				returnType = "Promise<" + returnType + ">";
		}

		output << ")" << ": " << returnType << ";\n";
	};

	std::function<void(nlohmann::json)> dumpType = [&](nlohmann::json data) -> void
	{
		if (ignored.contains(data["name"].get<std::string>()) || dumped.contains(data["name"].get<std::string>())) {
			return;
		}

		if (data["parent"]) {
			dumpType(data["parent"]);
		}

		output << "\n// Based on " << prettify(data["name"].get<std::string>()) << ".pex\n";
		output << "export declare class " << prettify(data["name"].get<std::string>()) << "extends" << (data["parent"] ? prettify(data["parent"]["name"]) : "PapyrusObject") << "\n";
		output << tab << "static from(papyrusObject: PapyrusObject | null) : " << prettify(data["name"].get<std::string>()) << "| null; \n";
		
		for (auto& function : data["memberFunctions"])
		{
			dumpFunction(data["name"].get<std::string>(), function, false);
		}
		for (auto& function : data["globalFunctions"])
		{
			dumpFunction(data["name"].get<std::string>(), function, true);
		}

		output << "}\n";
		dumped.insert(data["name"].get<std::string>());
	};

	if (!j["types"]["WorldSpace"])
	{
		j["types"]["WorldSpace"]["parent"] = "Form";
		j["types"]["WorldSpace"]["globalFunctions"] = nlohmann::json::array();
		j["types"]["WorldSpace"]["memberFunctions"] = nlohmann::json::array();
	};

	for (auto& typeName : j["types"].items()) 
	{
		auto& data = j["types"]["typeName"];
		if (data["parent"]) 
		{
			j["types"]["typeName"]["parent"] = j["types"][data["parent"].get<std::string>()];
		}
		j["types"]["typeName"]["name"] = typeName.key();
	}

	for (auto& typeName : j["types"]) \
	{
		dumpType(typeName);
	}

	output << getPostfix();
	output.close();
	return 0;
}
