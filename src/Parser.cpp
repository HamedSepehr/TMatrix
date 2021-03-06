/*
 * Copyright (C) 2018-2019 Miloš Stojanović
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "Parser.h"

namespace Parser {
	std::string Option::GetValueName(OptionType type)
	{
		switch (type) {
		case VERSION:
		case HELP:
			return "";
			break;
		case MODE:
			return "<mode>";
			break;
		case NUMERIC:
			return "<value>";
			break;
		case RANGE:
			return "<min>,<max>";
			break;
		case COLOR:
			return "<color>";
			break;
		case TEXT:
			return "<text>";
			break;
		}
		return "";
	}

	std::string Option::GetUsage() const
	{
		std::string valueName {GetValueName(Type)};
		switch (Type) {
		case VERSION:
		case HELP:
			return "[" + LongLiteral + ']';
			break;
		case MODE:
		case NUMERIC:
		case RANGE:
		case COLOR:
		case TEXT:
			if (ShortLiteral != "" && LongLiteral != "") {
				return "[" + ShortLiteral + valueName + " | " +
				       LongLiteral + "=" + valueName + "]";
			} else if (ShortLiteral != "") {
				return "[" + ShortLiteral + valueName + "]";
			} else {
				return "[" + LongLiteral + "=" + valueName + "]";
			}
			break;
		}
		return "";
	}

	std::string Option::GetLiterals() const
	{
		std::string valueName {GetValueName(Type)};
		switch (Type) {
		case VERSION:
		case HELP:
			return LongLiteral;
			break;
		case MODE:
		case NUMERIC:
		case RANGE:
		case COLOR:
		case TEXT:
			if (ShortLiteral != "" && LongLiteral != "") {
				return ShortLiteral + ", " + LongLiteral + "=" + valueName;
			} else if (ShortLiteral != "") {
				return ShortLiteral + " " + valueName;
			} else {
				return LongLiteral + "=" + valueName;
			}
			break;
		}
		return "";
	}

	std::pair<std::string_view, std::string_view> Option::GetPrefixSuffixSplit(std::string_view argument) const
	{
		std::string literal;
		if (StartsWith(argument, "--") && LongLiteral != "") {
			literal = LongLiteral;
			if (Type == MODE ||
			    Type == NUMERIC ||
			    Type == RANGE ||
			    Type == COLOR ||
			    Type == TEXT) {
				literal += '=';
			}
		} else if (StartsWith(argument, "-") && ShortLiteral != "") {
			literal = ShortLiteral;
		} else {
			throw std::invalid_argument("Invalid argument format.");
		}

		if (argument.size() < literal.size()) {
			throw std::invalid_argument("Argument is too big.");
		}
		std::string_view prefix {argument.substr(0, literal.length())};
		std::string_view suffix {argument.substr(literal.length())};
		if (prefix != literal) {
			throw std::invalid_argument("Prefix doesn't match.");
		}
		return {prefix, suffix};
	}

	bool Option::MatchesArgument (std::string_view argument) const
	{
		try {
			GetPrefixSuffixSplit(argument);
		} catch (const std::invalid_argument&) {
			return false;
		}
		return true;
	}

	//---Parser-functions---------------------------------------------------
	bool StartsWith(std::string_view str, std::string_view prefix) {
		return (str.substr(0, prefix.length()) == prefix);
	}

	bool ParseCmdLineArgs(std::vector<std::string_view> arguments,
			      int &stepsPerSecond, RainProperties &rainProperties)
	{
		SetRainProperties("default", rainProperties);
		for (decltype(arguments)::size_type i = 0; i < arguments.size(); i++) {
			std::string_view argument {arguments[i]};

			bool matched {false};
			for (const Option &option : Options) {
				if (!option.MatchesArgument(argument)) {
					continue;
				}
				auto [prefix, suffix] { option.GetPrefixSuffixSplit(argument) };
				matched = true;
				switch (option.Type) {
				case VERSION:
				case HELP:
					if (suffix != "") {
						matched = false;
						break;
					}
					option.ProcessArgument(argument, stepsPerSecond, rainProperties);
					return false;
					break;
				case MODE:
				case NUMERIC:
				case RANGE:
				case COLOR:
				case TEXT:
					if (suffix == "") {
						try {
							suffix = arguments.at(++i);
						} catch (const std::out_of_range&) {
							std::cout << "No value specified for " << argument << '\n';
							PrintUsage(false);
							return false;
						}
						if (StartsWith(prefix, "--")) {
							std::cout << "No value specified for " << argument << '\n';
							PrintUsage(false);
							return false;
						}
					}

					try {
						option.ProcessArgument(suffix, stepsPerSecond, rainProperties);
					} catch (const std::out_of_range&) {
						PrintInvalidValue(prefix, suffix);
						return false;
					} catch (const std::invalid_argument&) {
						PrintInvalidValue(prefix, suffix);
						return false;
					} catch (const std::range_error&) {
						std::cout << "Invalid values '" << suffix;
						std::cout << "' specified for " << prefix << '\n';
						std::cout << "The first value must be";
						std::cout << " greater than the second." << '\n';
						return false;
					}
					break;
				}
			}
			if (!matched) {
				std::cout << "Unknown option: " << argument << '\n';
				PrintUsage(false);
				return false;
			}
		}
		return true;
	}

	void PrintInvalidValue(std::string_view prefix, std::string_view suffix)
	{
		std::cout << "Invalid value '" << suffix;
		std::cout << "' specified for " << prefix << '\n';
		std::cout << "Try 'tmatrix --help' for more information." << '\n';
	}

	void ParseRuntimeInput(char c, bool &paused)
	{
		switch (c) {
		case 'p':
		case 'P':
			paused = !paused;
			break;
		case 'q':
		case 'Q':
			std::raise(SIGTERM);
			break;
		}
	}

	//---VERSION------------------------------------------------------------
	void PrintVersion()
	{
		std::cout.precision(1);
		std::cout << "tmatrix version " << std::fixed << VERSION_NUMBER << '\n';
		std::cout << '\n';
		std::cout << "Copyright (C) 2018-2019 Miloš Stojanović" << '\n';
		std::cout << "SPDX-License-Identifier: GPL-2.0-only" << '\n';
	}

	//---HELP---------------------------------------------------------------
	void PrintUsage(bool full)
	{
		std::vector<std::string> header {CreateUsageHeader()};
		for (std::string_view line : header) {
			std::cout << line << '\n';
		}

		if (full) {
			std::cout << "Simulates the digital rain effect from The Matrix." << '\n';
			std::cout << "Use 'p' to pause and 'q' to quit." << '\n';
			std::cout << '\n';

			std::size_t longestLiterals {FindLongestLiteralsLength()};
			for (const Option &option : Options) {
				if (option.Type != HELP && option.Type != VERSION) {
					PrintUsageLine(option, longestLiterals);
				}
			}
			// Print help and version at the end
			PrintSpecificOptionType(HELP, longestLiterals);
			PrintSpecificOptionType(VERSION, longestLiterals);
		}
	}

	std::vector<std::string> CreateUsageHeader()
	{
		std::vector<std::string> usage {"Usage: tmatrix"};
		const auto usageStartLength {usage.back().length()};

		for (const Option &option : Options) {
			if (usage.back().length() + 1 + option.GetUsage().length() > MAX_LINE_LENGTH) {
				usage.emplace_back(usageStartLength, ' ');
			}
			usage.back() += ' ' + option.GetUsage();
		}

		return usage;
	}

	std::size_t FindLongestLiteralsLength()
	{
		std::size_t longest {0};
		for (const Option &option : Options) {
			std::size_t current {option.GetLiterals().length()};
			current += option.HasShortLiteral() ? SHORT_GAP_PREFIX : LONG_GAP_PREFIX;
			if (current > longest) {
				longest = current;
			}
		}

		return longest;
	}

	void PrintUsageLine(const Option &option, std::size_t longestLiterals)
	{
		int gapSize {option.HasShortLiteral() ? SHORT_GAP_PREFIX : LONG_GAP_PREFIX};
		std::string line(gapSize, ' ');
		line += option.GetLiterals();
		line += std::string(longestLiterals-line.length(), ' ');
		line += SEPARATOR;
		for (std::size_t i = 0; i < option.HelpText.size(); i++) {
			if (i != 0) {
				line += std::string(longestLiterals+SEPARATOR.length(), ' ');
			}
			line += option.HelpText[i];
			line += '\n';
		}
		std::cout << line;
	}

	void PrintSpecificOptionType(OptionType type, std::size_t longestLiterals)
	{
		for (const Option &option : Options) {
			if (option.Type == type) {
				PrintUsageLine(option, longestLiterals);
			}
		}
	}

	//---STEPS-PER-SECONDS--------------------------------------------------
	void SetStepsPerSecond(std::string_view value, int &stepsPerSecond)
	{
		stepsPerSecond = ReturnValidNumber(value);
		if (stepsPerSecond < MIN_STEPS_PER_SECOND ||
		    stepsPerSecond > MAX_STEPS_PER_SECOND) {
			throw std::out_of_range("Value is out of range.");
		}
	}

	int ReturnValidNumber(std::string_view value)
	{
		if (value.length() == 0 ||
		    std::any_of(value.begin(), value.end(),
		    [](unsigned char c) { return !std::isdigit(c); })) {
			throw std::invalid_argument("Number isn't valid.");
		}

		return std::atoi(value.data());
	}

	//----------------------------------------------------------------------
	void SetRainProperties(std::string_view mode, RainProperties &rainProperties)
	{
		if (mode == "default") {
			rainProperties = Rain::DEFAULT_PROPERTIES;
		} else if (mode == "dense") {
			rainProperties = Rain::DENSE_PROPERTIES;
		} else {
			throw std::invalid_argument("Mode isn't valid.");
		}
	}

	//---RANGE--------------------------------------------------------------
	Range<int> SplitRange(std::string_view range)
	{
		auto commaPlace {range.find(',')};
		if (commaPlace == std::string_view::npos) {
			throw std::invalid_argument("No comma found in range argument.");
		}
		int min {ReturnValidNumber(range.substr(0, commaPlace))};
		int max {ReturnValidNumber(range.substr(commaPlace + 1))};
		return {min, max};
	}
	//---SPEED--------------------------------------------------------------
	void SetSpeedRange(std::string_view range, RainProperties &rainProperties)
	{
		rainProperties.RainColumnSpeed = SplitRange(range);
		if (rainProperties.RainColumnSpeed.GetMax() > Rain::MAX_FALL_SPEED) {
			throw std::out_of_range("Value is out of range.");
		}
	}
	//---STARTING-GAP-RANGE-------------------------------------------------
	void SetStartingGapRange(std::string_view range, RainProperties &rainProperties)
	{
		rainProperties.RainColumnStartingGap = SplitRange(range);
	}
	//---GAP-RANGE----------------------------------------------------------
	void SetGapRange(std::string_view range, RainProperties &rainProperties)
	{
		rainProperties.RainColumnGap = SplitRange(range);
	}
	//---LENGTH-------------------------------------------------------------
	void SetLengthRange(std::string_view range, RainProperties &rainProperties)
	{
		rainProperties.RainStreakLength = SplitRange(range);
		if (rainProperties.RainStreakLength.GetMin() < Rain::MIN_LENGTH) {
			throw std::out_of_range("Value is out of range.");
		}
	}
	//---CHARACTER-UPDATE-RATE----------------------------------------------
	void SetCharUpdateRateRange(std::string_view range, RainProperties &rainProperties)
	{
		rainProperties.MCharUpdateRate = SplitRange(range);
	}
	//---COLOR--------------------------------------------------------------
	void SetColor(std::string_view color, RainProperties &rainProperties)
	{
		if (color == "default") {
			rainProperties.Color = TerminalChar::DEFAULT_COLOR;
		} else if (color == "white") {
			rainProperties.Color = TerminalChar::WHITE_COLOR;
		} else if (color == "gray") {
			rainProperties.Color = TerminalChar::GRAY_COLOR;
		} else if (color == "black") {
			rainProperties.Color = TerminalChar::BLACK_COLOR;
		} else if (color == "red") {
			rainProperties.Color = TerminalChar::RED_COLOR;
		} else if (color == "green") {
			rainProperties.Color = TerminalChar::GREEN_COLOR;
		} else if (color == "yellow") {
			rainProperties.Color = TerminalChar::YELLOW_COLOR;
		} else if (color == "blue") {
			rainProperties.Color = TerminalChar::BLUE_COLOR;
		} else if (color == "magenta") {
			rainProperties.Color = TerminalChar::MAGENTA_COLOR;
		} else if (color == "cyan") {
			rainProperties.Color = TerminalChar::CYAN_COLOR;
		} else {
			throw std::invalid_argument("Color isn't valid.");
		}
	}
	//---BACKGROUND-COLOR---------------------------------------------------
	void SetBackgroundColor(std::string_view color, RainProperties &rainProperties)
	{
		if (color == "default") {
			rainProperties.BackgroundColor = TerminalChar::DEFAULT_BACKGROUND_COLOR;
		} else if (color == "white") {
			rainProperties.BackgroundColor = TerminalChar::WHITE_BACKGROUND_COLOR;
		} else if (color == "gray") {
			rainProperties.BackgroundColor = TerminalChar::GRAY_BACKGROUND_COLOR;
		} else if (color == "black") {
			rainProperties.BackgroundColor = TerminalChar::BLACK_BACKGROUND_COLOR;
		} else if (color == "red") {
			rainProperties.BackgroundColor = TerminalChar::RED_BACKGROUND_COLOR;
		} else if (color == "green") {
			rainProperties.BackgroundColor = TerminalChar::GREEN_BACKGROUND_COLOR;
		} else if (color == "yellow") {
			rainProperties.BackgroundColor = TerminalChar::YELLOW_BACKGROUND_COLOR;
		} else if (color == "blue") {
			rainProperties.BackgroundColor = TerminalChar::BLUE_BACKGROUND_COLOR;
		} else if (color == "magenta") {
			rainProperties.BackgroundColor = TerminalChar::MAGENTA_BACKGROUND_COLOR;
		} else if (color == "cyan") {
			rainProperties.BackgroundColor = TerminalChar::CYAN_BACKGROUND_COLOR;
		} else {
			throw std::invalid_argument("Bakcground color isn't valid.");
		}
	}
	//---TITLE--------------------------------------------------------------
	void SetTitle(std::string_view title, RainProperties &rainProperties)
	{
		rainProperties.Title = title.data();
	}
}
