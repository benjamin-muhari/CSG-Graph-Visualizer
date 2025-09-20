#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <iostream>
#include <map>

using CSGVMeasurment = std::pair<std::chrono::steady_clock::time_point, std::string>;

extern std::vector<CSGVMeasurment> measurements;
extern std::map<std::string,int> measurement_counts;

inline void DumpMeasurements()
{
	if (measurements.size() < 2)
		return;
	std::cout << "Timer measurements:\n";
	for (auto it = measurements.begin()+1; it != measurements.end(); it++)
	{
		auto took = (*it).first - (*(it-1)).first;
		auto took_ms = std::chrono::duration_cast<std::chrono::milliseconds>(took);
		std::string first_event = (*(it - 1)).second;
		std::string second_event = (*it).second;
		std::cout /*<< first_event*/ << " --> " << second_event << ": " << took_ms.count() << " ms\n";
		//std::cout << first_event << " #" << measurement_counts[first_event] << " --> "
		//	<< second_event << " #" << measurement_counts[second_event] << ": " << took_ms.count() << "ms\n";
	}
	std::cout << "\n";
	measurements.clear();
}

inline void InsertMeasurement(std::string message_key, bool auto_dump = false)
{
	std::string message;
	if (measurement_counts.find(message_key) == measurement_counts.end())
	{
		measurement_counts[message_key] = 1;
		message = message_key + " #1";
	}
	else
	{
		measurement_counts[message_key] = measurement_counts[message_key] + 1;
		message = message_key + " #" + std::to_string(measurement_counts[message_key]);
	}
	measurements.emplace_back(CSGVMeasurment(std::chrono::high_resolution_clock::now(), message));

	if (auto_dump && measurements.size() > 1)
		DumpMeasurements();
}

//struct MeasureClock
//{
//	//void SaveNow(std::string message);
//	void SaveNow(std::string message, bool display = true);
//	inline void SaveNow(const char* message) { SaveNow(std::string(message)); }
//	inline void SaveNow() { SaveNow(""); }
//};