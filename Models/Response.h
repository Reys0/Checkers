#pragma once

enum class Response
{
    OK, //Признак окончания хода
    BACK, // Откат на предыдущее состояние хода
    REPLAY, // Перезапуск игры
    QUIT, // Выход
    CELL //Выбор клетки
};
