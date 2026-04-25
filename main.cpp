#include <iostream>
#include <windows.h>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <thread>
#include <chrono>

using namespace ftxui;

uint64_t grid[32];
uint64_t nextGrid[32];
uint32_t skipRow = 0;

void setCell (int x, int y) {grid[x] |= (1ULL << y);}
void clearCell (int x, int y) {grid[x] &= ~(1ULL << y);}
bool getCell (int x, int y) {return (grid[x] & (1ULL << y)) != 0;}

void step(){
    memset(nextGrid, 0, sizeof(nextGrid));
    for (int i = 0; i < 32; i++) {
        if (grid[i] == 0 && (i - 1 < 0 || grid[i - 1] == 0) && (i + 1 >= 32 || grid[i + 1] == 0)) {
            skipRow |= (1ULL << i);
            continue;
        }
        break;
    }
    for (int i = 0; i < 32; i++) {
        //if (skipRow & (1ULL << i)) continue;
        for (int j = 0; j < 64; j++) {
            uint8_t cellNeighbours = 0;
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue;
                    int neighbourX = i + x;
                    int neighbourY = j + y;
                    if (neighbourX >= 0 && neighbourX < 32 && neighbourY >= 0 && neighbourY < 64) {
                        if (getCell(neighbourX, neighbourY)) cellNeighbours++;
                    }
                }
            }
            if(cellNeighbours == 3 || (cellNeighbours == 2 && getCell(i, j))) nextGrid[i] |= (1ULL << j);
        }
    }
    memcpy(grid, nextGrid, sizeof(grid));
}

int main() {

    // Gosper Glider Gun
setCell(2, 25);
setCell(3, 23); setCell(3, 25);
setCell(4, 13); setCell(4, 14); setCell(4, 21); setCell(4, 22); setCell(4, 35); setCell(4, 36);
setCell(5, 12); setCell(5, 16); setCell(5, 21); setCell(5, 22); setCell(5, 35); setCell(5, 36);
setCell(6, 1);  setCell(6, 2);  setCell(6, 11); setCell(6, 17); setCell(6, 21); setCell(6, 22);
setCell(7, 1);  setCell(7, 2);  setCell(7, 11); setCell(7, 15); setCell(7, 17); setCell(7, 18); setCell(7, 23); setCell(7, 25);
setCell(8, 11); setCell(8, 17); setCell(8, 25);
setCell(9, 12); setCell(9, 16);
setCell(10, 13); setCell(10, 14);

    auto screen = ScreenInteractive::Fullscreen();
    auto duration = std::chrono::duration<double, std::micro>(0);
    auto averageDuration = std::chrono::duration<double, std::micro>(0);
    int stepCount = 0;
    std::thread([&] {
        while (true) {
            auto start = std::chrono::high_resolution_clock::now();
            step();
            auto end = std::chrono::high_resolution_clock::now();
            stepCount++;
            duration = std::chrono::duration<double, std::micro>(end - start);
            averageDuration = ((averageDuration * (stepCount - 1)) + duration) / stepCount;
            screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }).detach();

    auto renderer = ftxui::Renderer([&] {
        Elements rows;
        for (int i = 0; i < 32; i++) {
            Elements cells;
            for (int j = 0; j < 64; j++) {
                cells.push_back(getCell(i, j) ? text("O") : text(" "));
            }
            rows.push_back(hbox(cells));
        }
        return hbox(
            border(vbox(rows)),
            vbox({
                text("Conway's Game of Life"),
                text("Gosper Glider Gun"),
                text("Press Ctrl+C to exit"),
                text("stepTime: " + std::to_string(duration.count()) + " us"),
                text("Average time per step: " + std::to_string(averageDuration.count()) + " us")
            }));
    });

    screen.Loop(renderer);
    return 0;
}