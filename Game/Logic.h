#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config) // Конфиг бота, подсчет бота, оптимизация ботa
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color) { // Поиск лучших ходов
        next_move.clear(); // очистка состояний движения
        next_best_state.clear(); //очистка состояния

        find_first_best_turn(board->get_board(), color, -1, -1, 0); //координаты

        vector<move_pos> res; // Вектор для ходов
        int state = 0; //Начальное состояние
        do {
            res.push_back(next_move[state]); //След ход из текущего состояния
            state = next_best_state[state]; // Переход в след состояние
        } 
        while (state != -1 || next_move[state].x != -1); //Проверка возможностей хода после съедания 
        return res;
    }


private:
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const // Ход шашки с возвратов в матрицу
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const // Калькулятор очков
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0; 
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        if (!first_bot_color) // Перемена, если первым ходил бот
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)
            return INF;
        if (b + bq == 0)
            return 0;
        int q_coef = 4; //Коэф для дамок
        if (scoring_mode == "NumberAndPotential") 
        {
            q_coef = 5; // Коэф для дамок, если используется вариант выше
        }
        return (b + bq * q_coef) / (w + wq * q_coef); //Возврат с расчетом очков
    }

    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state, // поиск первого лучшего хода
        double alpha = -1) {
        next_move.emplace_back(-1, -1, -1, -1); // Пустой ход
        next_best_state.push_back(-1);
        if (state != 0) {
            find_turns(x, y, mtx);
        }
        auto now_turns = turns;
        auto now_have_beats = have_beats; //Копии доступных ходов

        if (!now_have_beats && state != 0) { // Состояние, где больше нет ходов
           return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }
        double best_score = 1;
        for (auto turn : now_turns){ //Перебор всех ходов для движения шашки
            size_t new_state = next_move.size(); //Переменная нового состояния
            double score;
            if (now_have_beats) {
                find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, new_state, best_score); // Продолжение хода после съедание шашки
            }
            else {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score); //Никого не съедаем и переходим в рекурсию

            }
            if (score > best_score) { // Проверка результата, нахождение оптимального хода
                best_score = score;
                next_move[state] = turn;
                next_best_state[state] = (now_have_beats ? new_state : -1);
            }
        }
        return best_score; //Возврат результата
    }

    
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1, // поиск лучшего хода после предыдущего лучшего хода
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1) {
        if (depth == Max_depth) {
            return calc_score(mtx, (depth % 2 == color)); // Возвращение подсчета очков
        }
        if (x != -1) {
            find_turns(x, y, mtx); // Ищем серию пробитий
        }
        else {
            find_turns(color, mtx); // Возможные ходы от цвета
        }
        auto now_turns = turns;
        auto now_have_beats = have_beats;
        if (!now_have_beats && x != -1) {
           return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta); //Поиск наилучшего хода в рекурсии
        }

        if (turns.empty()) {
            return (depth % 2 ? 0 : INF); //Проверка есть ли ходы, проигрыш из-за отсутвия ходов или выигрыш
        }

        double min_score = INF + 1; //Минмакс алгоритм
        double max_score = -1;
        for (auto turn : now_turns) {
            double score;
            if (now_have_beats) {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2); // Если есть возможность съесть, продолжение хода, если нет, возврат в рекурсию
            }
            else {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta); //Если нет возможности съесть
            }
            min_score = min(min_score, score);
            max_score = max(max_score, score);
            
            if (depth & 2) { // Альфа и бета отсечения, чтобы бот работал быстрее и оптимизированние
                alpha = max(alpha, max_score);
            }
            else {
                beta = min(beta, min_score);
            }
            if (optimization != "00" && alpha == beta) { //Строгое отсечение
                break;
            }
            if (optimization == "02" && alpha == beta) { //Не строгое отсечение
                return (depth % 2 ? max_score + 1 : min_score - 1);
            }
        }
        return (depth % 2 ? max_score : min_score); //Определение хода нашего или нет
    }

    
public:
    void find_turns(const bool color) // Поиск хода по цвету
    {
        find_turns(color, board->get_board());
    }

    void find_turns(const POS_T x, const POS_T y) // Поиск хода по позиции
    {
        find_turns(x, y, board->get_board());
    }

private:
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx) // поиск ходов от цвета и матрицы
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false; // Движение шашки с возможностью съесть
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx) // поиск хода от позиции
    {
        turns.clear(); // Логика движений шашки и дамки
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public: // Ходы с съеданием шашки и макс глубина хода бота
    vector<move_pos> turns;
    bool have_beats;
    int Max_depth;

  private: //Указатели на ориг
    default_random_engine rand_eng;
    string scoring_mode;
    string optimization;
    vector<move_pos> next_move;
    vector<int> next_best_state;
    Board *board;
    Config *config;
};
