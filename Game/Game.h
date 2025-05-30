#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        auto start = chrono::steady_clock::now();
        if (is_replay) //Если игру начали с начала
        {
            logic = Logic(&board, &config); // Отчистить логику бота
            config.reload(); //Перевыгрузить конфиг
            board.redraw(); // Отрисовать доску сначала
        }
        else
        {
            board.start_draw(); //Первичная отрисовка доски
        }
        is_replay = false;

        int turn_num = -1;
        bool is_quit = false;
        const int Max_turns = config("Game", "MaxNumTurns");
        while (++turn_num < Max_turns) //Пока кол-во ходов меньше максимального
        {
            beat_series = 0;
            logic.find_turns(turn_num % 2);
            if (logic.turns.empty())
                break;
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot"))) //Если сейчас ходит не бот
            {
                auto resp = player_turn(turn_num % 2); // Получить действие игрока
                if (resp == Response::QUIT) // Действие выход
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY) // Действие запуска игры с начала
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK) // Действие отмены предыдущего хода
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else //Иначе ход бота
                bot_turn(turn_num % 2);
        }
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        if (is_replay)
            return play();
        if (is_quit)
            return 0;
        int res = 2; // 2, значит победили белые
        if (turn_num == Max_turns)
        {
            res = 0; // 0, значит вышли за макс кол-во ходов, ничья
        }
        else if (turn_num % 2) //1, значит победили черные
        {
            res = 1;
        }
        board.show_final(res); //Финальный экран
        auto resp = hand.wait(); //Запрос действия от игрока на продолжение игры
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }
        return res;
    }

  private:
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now();

        auto delay_ms = config("Bot", "BotDelayMS"); //Получить таймаут действия бота
        // new thread for equal delay for each turn
        thread th(SDL_Delay, delay_ms); // Эмуляция таймаута
        auto turns = logic.find_best_turns(color); //Получение лучших ходов 
        th.join();
        bool is_first = true;
        // making moves
        for (auto turn : turns)
        {
            if (!is_first) //Если ход бота не первый, попытка сделaть таймаут
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            beat_series += (turn.xb != -1);
            board.move_piece(turn, beat_series);
        }
        //Записи хода в журнал
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)
    {
        // return 1 if quit
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns) //Для каждого возможного хода
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells); //Подсветит возможные для хода клетки
        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;
        // trying to make first move
        while (true)
        {
            auto resp = hand.get_cell(); // Попытка получить клетку
            if (get<0>(resp) != Response::CELL) // Если результат не клетка, вернуть результат
                return get<0>(resp);
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)}; // Иначе получить клетку

            bool is_correct = false;
            for (auto turn : logic.turns) //Для каждого возможного хода
            {
                if (turn.x == cell.first && turn.y == cell.second) // Если выбранная ячейка совпадает с возможным ходом
                {
                    is_correct = true; //Считаем ход коректным
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second}) 
                {
                    pos = turn;
                    break;
                }
            }
            if (pos.x != -1) //Если не выбрана ячейка с шашкой
                break; 
            if (!is_correct) // Если действие некоректное
            {
                if (x != -1) // Если не выбрана ячейка с шашкой
                { //Отчистить выбранную ячейку, перерисовать подсветку
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }
            x = cell.first; //х и у заполняется координатами ячейки
            y = cell.second;
            board.clear_highlight(); //Убирается подсветка
            board.set_active(x, y); //Шашка становится активной
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns) // Для каждого возможного хода с новой позиции
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2); //Подсветить возможные ходы
        }
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1); // Передвинуть шашку
        if (pos.xb == -1) // Не было съедено не одной шашки
            return Response::OK;
        // continue beating while can
        beat_series = 1;
        while (true) 
        {
            logic.find_turns(pos.x2, pos.y2);
            if (!logic.have_beats) //Если больше нечего есть, выйти из цикла
                break;

            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns) //Иначе подсветить все возможные ходы
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);
            // trying to make move
            while (true)
            {
                auto resp = hand.get_cell(); //попытка получить клетку
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                bool is_correct = false;
                for (auto turn : logic.turns) //Проверка коректности выбранной ячейки
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                if (!is_correct) // Если клетка некоректная повторить действие
                    continue;

                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series); // Переместить шашку
                break;
            }
        }

        return Response::OK;
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
