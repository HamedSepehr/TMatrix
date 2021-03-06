/*
 * Copyright (C) 2018-2019 Miloš Stojanović
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <iostream>
#include <ncurses.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "Terminal.h"

const char *TerminalChar::GLOWING_COLOR_ESC_SEQ {TerminalChar::WHITE_COLOR};
const char *TerminalChar::NORMAL_COLOR_ESC_SEQ {nullptr};

int Terminal::NumberOfRows {0};
int Terminal::NumberOfColumns {0};
std::vector<TerminalChar> Terminal::ScreenBuffer {};

Terminal::Terminal(const char *color, const char *background_color)
{
	initscr();
	savetty();
	nonl();
	cbreak();
	noecho();
	timeout(0);
	leaveok(stdscr, TRUE);
	curs_set(0);

	// Disable C stream sync
	std::ios::sync_with_stdio(false);

	// Set bold style
	std::cout << "\033[1m";
	// Set foreground color
	TerminalChar::SetColor(color);
	// Set background color
	std::cout << background_color;

	Terminal::Reset();

	// Calling this here first prevents delay in the main loop.
	getch();
}

Terminal::~Terminal()
{
	curs_set(1);
	clear();
	refresh();
	resetty();
	endwin();
}

void Terminal::Reset()
{
	struct winsize windowSize;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize);
	NumberOfRows = windowSize.ws_row;
	NumberOfColumns = windowSize.ws_col;

	ScreenBuffer = std::vector(NumberOfColumns*NumberOfRows, TerminalChar());
}

void Terminal::Draw(int x, int y, const char *mchar, bool isGlowing)
{
	if (x < 0 || x > NumberOfColumns-1 || y < 0 || y > NumberOfRows-1) {
		return;
	}

	ScreenBuffer[y*NumberOfColumns + x].SetFullMChar(mchar, isGlowing);
}

void Terminal::Erase(int x, int y)
{
	if (x < 0 || x > NumberOfColumns-1 || y < 0 || y > NumberOfRows-1) {
		return;
	}

	ScreenBuffer[y*NumberOfColumns + x].Clear();
}

void Terminal::DrawTitle(int x, int y, char tchar)
{
	if (x < 0 || x > NumberOfColumns-1 || y < 0 || y > NumberOfRows-1) {
		return;
	}

	ScreenBuffer[y*NumberOfColumns + x].SetFullTitleChar(tchar);
}

void Terminal::Flush()
{
	std::cout.write(reinterpret_cast<char*>(ScreenBuffer.data()),
			ScreenBuffer.size() * sizeof(decltype(ScreenBuffer)::value_type));
	std::cout << std::flush;
	// Move cursor to the start of the screen
	std::cout << "\033[0;0H";
}

char Terminal::ReadInputChar()
{
	return getch();
}
