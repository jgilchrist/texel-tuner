#ifndef TCHERAN_H
#define TCHERAN_H

#define TAPERED 1

#include "../base.h"
#include "../external/chess.hpp"

#include <string>
#include <vector>

namespace Tcheran
{
    class TcheranEval
    {
    public:
        constexpr static bool includes_additional_score = false;
        constexpr static bool supports_external_chess_eval = true;
        constexpr static bool retune_from_zero = true;
        constexpr static tune_t preferred_k = 2.5;
        constexpr static int32_t max_epoch = 5001;
        constexpr static bool enable_qsearch = false;
        constexpr static bool filter_in_check = false;
        constexpr static tune_t initial_learning_rate = 1;
        constexpr static int32_t learning_rate_drop_interval = 10000;
        constexpr static tune_t learning_rate_drop_ratio = 1;
        constexpr static bool print_data_entries = false;
        constexpr static int32_t data_load_print_interval = 100000;


        static parameters_t get_initial_parameters();

        static EvalResult get_fen_eval_result(const std::string &fen)
        {
            chess::Board board;
            board.setFen(fen);
            return get_external_eval_result(board);
        }

        static EvalResult get_external_eval_result(const chess::Board& board);

        static void print_parameters(const parameters_t& parameters);
    };
}

#endif // TCHERAN_H

