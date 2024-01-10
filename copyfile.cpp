#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <omp.h>
namespace fs = std::filesystem;

unsigned activeOptionCount = 0;

enum optionList {
	TARGET,
	VERBOSE,
	DEBUG,
	PARALLEL,
	TIMER
};

std::unordered_map<std::wstring, optionList> optionKeys = {
	{L"-t", TARGET},
	{L"-v", VERBOSE},
	{L"-d", DEBUG},
	{L"-p", PARALLEL},
	{L"-T", TIMER}
};

std::unordered_map<optionList, bool> options = {
	{TARGET, false},
	{VERBOSE, false},
	{DEBUG, false},
	{PARALLEL, false},
	{TIMER, false}
};

void checkOptions() {
	unsigned numberOptions = options.size();
	if (__argc - 1 < numberOptions) {
		numberOptions = __argc - 1;
	}

	std::unordered_map<std::wstring, optionList>::iterator currentOption;
	for (unsigned i = 1; i < numberOptions + 1; i++) {
		if ((currentOption = optionKeys.find(__wargv[i])) != optionKeys.end()) {
			options.at((*currentOption).second) = true;
			activeOptionCount++;
		}
	}
}

void parseArgs(std::vector<fs::path>& files, fs::path& correctDestinationFile) {
	unsigned lastFileIndex = __argc - 1;
	unsigned firstFileIndex = activeOptionCount + 1;
	if (options.at(TARGET)) {
		correctDestinationFile = fs::absolute(fs::path(__wargv[__argc - 1]).lexically_normal());
		lastFileIndex--;

		if(options.at(DEBUG)) {
			if (fs::is_directory(correctDestinationFile)) {
				#pragma omp critical (wcout)
				{
					std::wcout << L"(debug): Директория назначения " << correctDestinationFile << L" задана" << std::endl;
				}
			} else {
				#pragma omp critical (wcout)
				{
					std::wcout << L"(debug): Файл назначения " << correctDestinationFile << L" задан" << std::endl;
				}
			}
		}
	
	}
	
	#pragma omp parallel if (options.at(PARALLEL))
	{
		#pragma omp for ordered
		for (unsigned i = firstFileIndex; i < lastFileIndex + 1; i++) {

			if (options.at(DEBUG)) {
				#pragma omp critical (wcout)
				{
					std::wcout << L"(debug): Поток " << omp_get_thread_num() << L" проверяет агрумент " << i <<
						L" {" << __wargv[i] << L"}" << std::endl;
				}
			}

			if (!fs::exists(__wargv[i])) {
				#pragma omp critical (wcout)
				{
					std::wcout << L"Ошибка: Файл источник " << __wargv[i] << L" не существует!" << std::endl;
				}
				continue;
			}

			if (fs::is_directory(__wargv[i])) {
				#pragma omp critical (wcout)
				{
					std::wcout << L"Ошибка: Файл источник " << __wargv[i] << L" является директорией!" << std::endl;
				}
				continue;
			}

			#pragma omp ordered
    		files.emplace_back(fs::absolute(fs::path(__wargv[i]).lexically_normal()));

			if (options.at(DEBUG)) {
				#pragma omp critical (wcout)
				{
					std::wcout << L"(debug): Поток " << omp_get_thread_num() << L" добавил файл " <<
						__wargv[i] << L" в очередь" << std::endl;
				}
			}
		
		}
	}
}

fs::path destinationFileCorrection(fs::path sourceFile, fs::path destinationFile, const size_t& numberSourceFiles) {
	if (destinationFile.empty() or destinationFile == sourceFile.parent_path()) {
    	destinationFile = sourceFile.replace_filename(L"copy - " + sourceFile.filename().wstring());
	} else if (fs::is_directory(destinationFile)) {
		destinationFile += L"\\" + sourceFile.filename().wstring();
    } else if (numberSourceFiles > 1) {
		std::wcout << L"Ошибка: Невозможно множественно переименовать файлы!" << std::endl;
		exit(EXIT_FAILURE);
	}
	return destinationFile;
}

void programUsage() {
	std::wcout << L"Использование:" << L" .\\сopyfile.exe [ПАРАМЕТРЫ]... ФАЙЛ-ИСТОЧНИК...\n" <<
		std::setw(14) << 	L"или:" << L" .\\сopyfile.exe [ПАРАМЕТРЫ]... -t ФАЙЛ-ИСТОЧНИК... ДИРЕКТОРИЯ-НАЗНАЧЕНИЯ\n" <<
		std::setw(14) << 	L"или:" << L" .\\сopyfile.exe [ПАРАМЕТРЫ]... -t ФАЙЛ-ИСТОЧНИК ФАЙЛ-НАЗНАЧЕНИЯ\n" <<
		std::endl <<
		L"1) Копирует один или несколько ФАЙЛ-ИСТОЧНИК в ту же директорию\n" <<
		L"2) Копирует один или несколько ФАЙЛ-ИСТОЧНИК в ДИРЕКТОРИЯ-НАЗНАЧЕНИЯ c исходным именем\n" <<
		L"3) Копирует один ФАЙЛ-ИСТОЧНИК в ФАЙЛ-НАЗНАЧЕНИЯ с указанным именем\n" <<
		std::endl <<
		L"Допустимые ПАРАМЕТРЫ: -t :" << L" (target) Указывает директорию или файл назначения последним аргументом\n" <<
		std::setw(26) << 	  L"-v :" << L" (verbose) Выводит информацию о скопированных файлах\n" <<
		std::setw(26) << 	  L"-d :" << L" (debug) Добавляет к выводу отладочную информацию\n" <<
		std::setw(26) << 	  L"-p :" << L" (parallel) Активирует режим параллельного копирования файлов\n" <<
		std::setw(26) << 	  L"-T :" << L" (timer) Выводит затраченное на выполнение время" << std::endl;
}

int wmain(int argc, wchar_t* argv[]) {
	setlocale(LC_ALL, "");

	std::vector<fs::path> files;
	fs::path destinationFile = L"";
	auto start = std::chrono::high_resolution_clock::now();

	if (argc > 1) {
		checkOptions();
		parseArgs(files, destinationFile);
    } else {
		programUsage();
		return EXIT_SUCCESS;
	}

	#pragma omp parallel if (options.at(PARALLEL))
	{
		#pragma omp for
		for (auto sourceFile = files.begin(); sourceFile != files.end(); sourceFile++) {
			fs::path correctDestinationFile = destinationFileCorrection(*sourceFile, destinationFile, files.size());

			if (fs::exists(correctDestinationFile)) {
				#pragma omp critical (wcout)
				{
					std::wcout << L"Ошибка: Файл назначения " << correctDestinationFile.filename() <<
						L" уже существует в целевой директории!" << std::endl;
				}
				continue;
			}

			try {
				fs::copy_file(*sourceFile, correctDestinationFile);
			} catch (const fs::filesystem_error &exeption) {
				#pragma omp critical (wcout)
				{
					std::wcout << exeption.what() << std::endl;
				}
				exit(exeption.code().value());
			}

			if (options.at(DEBUG)) {
				#pragma omp critical (wcout)
				{
					std::wcout << L"(debug): Поток " << omp_get_thread_num() << L" скопировал файл " <<
						(*sourceFile).filename() << std::endl;
				}
			}

			if (options.at(VERBOSE)) {
				#pragma omp critical (wcout)
				{
					std::wcout << L"Файл " << (*sourceFile).filename() << L" успешно скопирован!" << std::endl;
				}
			}
		}
	}

	auto finish = std::chrono::high_resolution_clock::now();
	if (options.at(TIMER)) {
		auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		std::wcout << L"Затраченное время: " << (double)(elapsedTime) / 1000.0 << L" с. (" <<
			elapsedTime << L" мс.)" << std::endl;
	}

	return EXIT_SUCCESS;
}
