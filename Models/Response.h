#pragma once

enum class Response
{
    OK, //Признак окончaния хода
    BACK, // Откат на предыдущее состояние хода
    REPLAY, // Перезапуск игры
    QUIT, // Выход
    CELL //Выбор клетки
};
