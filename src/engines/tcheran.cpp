#include "tcheran.h"

#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;
using namespace Tcheran;

using u64 = uint64_t;
using i32 = int;

struct Trace
{
    i32 score;

    i32 material[6][2]{};

    i32 pst_pawn[64][2]{};
    i32 pst_knight[64][2]{};
    i32 pst_bishop[64][2]{};
    i32 pst_rook[64][2]{};
    i32 pst_queen[64][2]{};
    i32 pst_king[64][2]{};

    i32 bishop_pair[2]{};
};

const i32 phases[] = {0, 1, 1, 2, 4, 0};

#define TRACE_INCR(parameter) trace.parameter[color]++
#define TRACE_ADD(parameter, count) trace.parameter[color] += count

struct EvalForColorResult {
    int score;
    int phase;
};

void trace_for_color(const chess::Board& board, chess::Color c, Trace& trace) {
    int color = c == chess::Color::WHITE ? 0 : 1;
    bool shouldFlip = c == chess::Color::BLACK;

    // Pawns
    auto pawns = board.pieces(chess::PieceType::PAWN, c);
    while (pawns) {
        auto sq = chess::Square(pawns.pop());
        if (shouldFlip) { sq.flip(); }

        TRACE_INCR(material[0]);
        TRACE_INCR(pst_pawn[sq.index()]);
    }

    // Knights
    auto knights = board.pieces(chess::PieceType::KNIGHT, c);
    while (knights) {
        auto sq = chess::Square(knights.pop());
        if (shouldFlip) { sq.flip(); }

        TRACE_INCR(material[1]);
        TRACE_INCR(pst_knight[sq.index()]);
    }

    // Bishops
    int bishop_count = 0;
    auto bishops = board.pieces(chess::PieceType::BISHOP, c);
    while (bishops) {
        auto sq = chess::Square(bishops.pop());
        if (shouldFlip) { sq.flip(); }

        bishop_count += 1;

        TRACE_INCR(material[2]);
        TRACE_INCR(pst_bishop[sq.index()]);
    }

    if (bishop_count > 1) {
        TRACE_INCR(bishop_pair);
    }

    // Rooks
    auto rooks = board.pieces(chess::PieceType::ROOK, c);
    while (rooks) {
        auto sq = chess::Square(rooks.pop());
        if (shouldFlip) { sq.flip(); }

        TRACE_INCR(material[3]);
        TRACE_INCR(pst_rook[sq.index()]);
    }

    // Queens
    auto queens = board.pieces(chess::PieceType::QUEEN, c);
    while (queens) {
        auto sq = chess::Square(queens.pop());
        if (shouldFlip) { sq.flip(); }

        TRACE_INCR(material[4]);
        TRACE_INCR(pst_queen[sq.index()]);
    }

    // Kings
    auto kings = board.pieces(chess::PieceType::KING, c);
    while (kings) {
        auto sq = chess::Square(kings.pop());
        if (shouldFlip) { sq.flip(); }

        TRACE_INCR(pst_king[sq.index()]);
    }
}

static Trace eval(const chess::Board& board) {
    Trace trace{};
    trace_for_color(board, chess::Color::WHITE, trace);
    trace_for_color(board, chess::Color::BLACK, trace);
    return trace;
}

static int32_t round_value(tune_t value)
{
    return static_cast<int32_t>(round(value));
}

static void rebalance_psts(parameters_t& parameters, const int32_t piece_index, const int32_t pst_offset, bool pawn_exclusion, const int32_t pst_size, const int32_t quantization)
{
    const int pstStart = pst_offset;
    for (int stage = 0; stage < 2; stage++)
    {
        double sum = 0;
        for (auto i = 0; i < pst_size; i++)
        {
            const auto sq = chess::Square(i);

            if (pawn_exclusion && (sq.rank() == chess::Rank(chess::Rank::RANK_1) || sq.rank() == chess::Rank(chess::Rank::RANK_8))) {
                continue;
            }

            const auto pstIndex = pstStart + i;
            sum += parameters[pstIndex][stage];
        }

        const auto average = sum / (!pawn_exclusion ? pst_size : pst_size - 16);

        parameters[piece_index][stage] += average * quantization;
        for (auto i = 0; i < pst_size; i++)
        {
            const auto sq = chess::Square(i);

            if (pawn_exclusion && (sq.rank() == chess::Rank(chess::Rank::RANK_1) || sq.rank() == chess::Rank(chess::Rank::RANK_8))) {
                continue;
            }

            const auto pstIndex = pstStart + i;
            parameters[pstIndex][stage] -= average;
        }
    }
}

static void print_parameter(std::stringstream& ss, const pair_t parameter)
{
    const auto mg = round_value(parameter[static_cast<int32_t>(PhaseStages::Midgame)]);
    const auto eg = round_value(parameter[static_cast<int32_t>(PhaseStages::Endgame)]);
    ss << "s(";
    ss << std::setw(5);
    ss << mg;
    ss << ", ";
    ss << std::setw(5);
    ss << eg;
    ss << ")";
}

static void print_single(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name)
{
    ss << "pub const " << name << ": PhasedEval = ";
    print_parameter(ss, parameters[index]);
    index++;

    ss << ";" << endl;
}

static void print_array(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name, const int count, const std::string& typeName)
{
    ss << "pub const " << name << ": [PhasedEval; " << typeName << "] = [" << endl;
    for (auto i = 0; i < count; i++)
    {
        ss << "    ";
        print_parameter(ss, parameters[index]);
        index++;

        if (i != count - 1)
        {
            ss << ", ";
        }

        ss << endl;
    }
    ss << "];" << endl << endl;
}

static void print_pst(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name)
{
    ss << "pub const " << name << ": PieceSquareTableDefinition = [" << "\n";

    for (auto rank = 7; rank >= 0; rank--) {
        ss << "    [";

        for (auto file = 0; file < 8; file++) {
            auto sq = chess::Square(chess::Rank(rank), chess::File(file));

            print_parameter(ss, parameters[index + sq.index()]);

            if (file != 7) {
                ss << ", ";
            }
        }

        ss << "],\n";
    }

    ss << "];\n\n";

    index += 64;
}

static void print_array_2d(std::stringstream& ss, const parameters_t& parameters, int& index, const std::string& name, const int count1, const int count2)
{
    ss << "pub const i32 " << name << "[][" << count2 << "] = {\n";
    for (auto i = 0; i < count1; i++)
    {
        ss << "    {";
        for (auto j = 0; j < count2; j++)
        {
            print_parameter(ss, parameters[index]);
            index++;

            if (j != count2 - 1)
            {
                ss << ", ";
            }
        }
        ss << "},\n";
    }
    ss << "};\n";
}

void get_zeros_parameter_array(parameters_t& parameters, const int size)
{
    for (int i = 0; i < size; i++)
    {
        get_initial_parameter_single(parameters, 0);
    }
}

parameters_t TcheranEval::get_initial_parameters()
{
    parameters_t parameters;

    // Material
    get_zeros_parameter_array(parameters, 6);

    // Pawn PST
    get_zeros_parameter_array(parameters, 64);

    // Knight PST
    get_zeros_parameter_array(parameters, 64);

    // Bishop PST
    get_zeros_parameter_array(parameters, 64);

    // Rook PST
    get_zeros_parameter_array(parameters, 64);

    // Queen PST
    get_zeros_parameter_array(parameters, 64);

    // King PST
    get_zeros_parameter_array(parameters, 64);

    // Bishop pair
    get_initial_parameter_single(parameters, 0);
    return parameters;
}

static coefficients_t get_coefficients(const Trace& trace)
{
    coefficients_t coefficients;

    get_coefficient_array(coefficients, trace.material, 6);

    get_coefficient_array(coefficients, trace.pst_pawn, 64);
    get_coefficient_array(coefficients, trace.pst_knight, 64);
    get_coefficient_array(coefficients, trace.pst_bishop, 64);
    get_coefficient_array(coefficients, trace.pst_rook, 64);
    get_coefficient_array(coefficients, trace.pst_queen, 64);
    get_coefficient_array(coefficients, trace.pst_king, 64);

    get_coefficient_single(coefficients, trace.bishop_pair);

    return coefficients;
}

void TcheranEval::print_parameters(const parameters_t& ps)
{
    parameters_t parameters = ps;
    rebalance_psts(parameters, 0, 6, true, 64, 1);
    rebalance_psts(parameters, 1, 6 + 64 * 1, false, 64, 1);
    rebalance_psts(parameters, 2, 6 + 64 * 2, false, 64, 1);
    rebalance_psts(parameters, 3, 6 + 64 * 3, false, 64, 1);
    rebalance_psts(parameters, 4, 6 + 64 * 4, false, 64, 1);
    // rebalance_psts(parameters, 5, 6 + 64 * 5, false, 64, 1);

    int index = 0;
    stringstream ss;
    print_array(ss, parameters, index, "PIECE_VALUES", 6, "PieceKind::N");
    print_pst(ss, parameters, index, "PAWNS");
    print_pst(ss, parameters, index, "KNIGHTS");
    print_pst(ss, parameters, index, "BISHOPS");
    print_pst(ss, parameters, index, "ROOKS");
    print_pst(ss, parameters, index, "QUEENS");
    print_pst(ss, parameters, index, "KING");
    print_single(ss, parameters, index, "BISHOP_PAIR_BONUS");
    cout << ss.str() << "\n";
}

EvalResult TcheranEval::get_external_eval_result(const chess::Board& board)
{
    const auto trace = eval(board);
    EvalResult result;
    result.coefficients = get_coefficients(trace);
    result.score = trace.score;

    return result;
}
