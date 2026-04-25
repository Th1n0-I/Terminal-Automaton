#include <iostream>
#include <windows.h>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <thread>
#include <chrono>
#include <mutex>

using namespace ftxui;

int GRID_WIDTH = 1024;
int GRID_HEIGHT = 1024;
int GRID_CHUNKS = (GRID_WIDTH + 63) / 64;

int stepCount = 0;

std::vector<std::vector<uint64_t>> grid;
std::vector<std::vector<uint64_t>> nextGrid;

std::mutex gridMutex;

bool deadRowSkipping = false;

void GenerateGrid() {
    grid.assign(GRID_HEIGHT, std::vector<uint64_t>(GRID_CHUNKS, 0));
    nextGrid.assign(GRID_HEIGHT, std::vector<uint64_t>(GRID_CHUNKS, 0));
}

void setCellGrid (int x, int y) {grid[x][y / 64] |= (1ULL << (y % 64));}
void clearCellGrid (int x, int y) {grid[x][y / 64] &= ~(1ULL << (y % 64));}
bool getCellGrid (int x, int y) {return (grid[x][y / 64] & (1ULL << (y % 64))) != 0;}

bool isRowEmpty (int x) {
    for (auto& chunk : grid[x]) {
        if (chunk != 0) return false;
    }
    return true;
}

void step(){
    stepCount++;
    for(auto& row : nextGrid) std::fill(row.begin(), row.end(), 0);
    for (int i = 0; i < GRID_HEIGHT; i++) {
        bool above = (i > 0) && !isRowEmpty(i - 1);
        bool current = !isRowEmpty(i);
        bool below = (i < GRID_HEIGHT - 1) && !isRowEmpty(i + 1);
        if (!above && !current && !below && deadRowSkipping) continue;

        for (int c = 0; c < GRID_CHUNKS; c++) {
            uint64_t activity = 0;
            if (i > 0) activity |= grid[i - 1][c];
            activity |= grid[i][c];
            if (i < GRID_HEIGHT - 1) activity |= grid[i + 1][c];

            if (c > 0) {
                if (i > 0) activity |= (grid[i - 1][c - 1]);
                activity |= (grid[i][c - 1]);
                if (i < GRID_HEIGHT - 1) activity |= (grid[i + 1][c - 1]);
            }

            if (c < GRID_CHUNKS - 1) {
                if (i > 0) activity |= (grid[i - 1][c + 1]);
                activity |= (grid[i][c + 1]);
                if (i < GRID_HEIGHT - 1) activity |= (grid[i + 1][c + 1]);
            }
            if (activity == 0 && deadRowSkipping) {
                c++; // Skip next chunk as well since it can't have any activity
                continue;
            }
        }

        for(int j = 0; j < GRID_WIDTH; j++) {
            uint8_t cellNeighbours = 0;
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue;
                    int neighbourX = i + x;
                    int neighbourY = j + y;
                    if (neighbourX >= 0 && neighbourX < GRID_HEIGHT && neighbourY >= 0 && neighbourY < GRID_WIDTH) {
                        if (getCellGrid(neighbourX, neighbourY)) cellNeighbours++;
                    }
                }
            }
            if (cellNeighbours == 3 || (cellNeighbours == 2 && getCellGrid(i, j))) nextGrid[i][j / 64] |= (1ULL << (j % 64));
        }
    }
}

int main() {
    GenerateGrid();

    // Gosper Glider Gun
    setCellGrid(2, 25);
    setCellGrid(3, 23); setCellGrid(3, 25);
    setCellGrid(4, 13); setCellGrid(4, 14); setCellGrid(4, 21); setCellGrid(4, 22); setCellGrid(4, 35); setCellGrid(4, 36);
    setCellGrid(5, 12); setCellGrid(5, 16); setCellGrid(5, 21); setCellGrid(5, 22); setCellGrid(5, 35); setCellGrid(5, 36);
    setCellGrid(6, 1);  setCellGrid(6, 2);  setCellGrid(6, 11); setCellGrid(6, 17); setCellGrid(6, 21); setCellGrid(6, 22);
    setCellGrid(7, 1);  setCellGrid(7, 2);  setCellGrid(7, 11); setCellGrid(7, 15); setCellGrid(7, 17); setCellGrid(7, 18); setCellGrid(7, 23); setCellGrid(7, 25);
    setCellGrid(8, 11); setCellGrid(8, 17); setCellGrid(8, 25);
    setCellGrid(9, 12); setCellGrid(9, 16);
    setCellGrid(10, 13); setCellGrid(10, 14);



    auto screen = ScreenInteractive::Fullscreen();
    double duration = 0.0;
    double averageDuration = 0.0;
    int stepCount = 0;
    std::thread([&] {
        while (true) {
            auto start = std::chrono::high_resolution_clock::now();
            step();
            auto end = std::chrono::high_resolution_clock::now();
            std::lock_guard<std::mutex> lock(gridMutex);
            grid = nextGrid;
            stepCount++;
            duration = std::chrono::duration<double, std::micro>(end - start).count();
            averageDuration = ((averageDuration * (stepCount - 1)) + duration) / stepCount;
        }
    }).detach();

    std::thread([&] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(330));
            screen.PostEvent(Event::Custom);
        }
    }).detach();

    auto renderer = ftxui::Renderer([&] {

        auto termWidth = Terminal::Size().dimx;
        auto termHeight = Terminal::Size().dimy;

        int visibleColls = min(GRID_WIDTH, termWidth);
        int visibleRows = min(GRID_HEIGHT, termHeight);

        std::lock_guard<std::mutex> lock(gridMutex);
        Elements rows;
        for (int i = 0; i < visibleRows; i++) {
            Elements cells;
            for (int j = 0; j < visibleColls; j++) {
                cells.push_back(getCellGrid(i, j) ? text("O") : text(" "));
            }
            rows.push_back(hbox(cells));
        }
        return hbox(
            border(vbox(rows)) | flex,
            border(vbox({
                text("Conway's Game of Life"),
                text("Gosper Glider Gun"),
                text("Press Ctrl+C to exit"),
                text("stepTime: " + std::to_string(duration / 1000.0) + " ms"),
                text("Average time per step: " + std::to_string(averageDuration / 1000.0) + " ms"),
                text("Step: " + std::to_string(stepCount)),
                hbox({
                    text("Dead Row Skipping: "),
                    text(deadRowSkipping ? "ON" : "OFF") | color(deadRowSkipping ? Color::Green : Color::Red)
                })
            })));
    });

    screen.Loop(renderer);

    return 0;
}