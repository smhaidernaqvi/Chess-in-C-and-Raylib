// ============================================================
//  CHESS GAME — C++ OOP + Raylib + Minimax AI Bot
//
// by Syed Muhammad Haider Naqvi (32394) and Syed Khurram Abbas Rizvi (33554)
//
//  OOP Concepts Used:
//  1. Abstract Base Class      (Piece)
//  2. Inheritance              (Pawn, Knight, Bishop, Rook, Queen, King)
//  3. Polymorphism             (virtual getValidMoves())
//  4. Encapsulation            (private data + getters/setters)
//  5. Operator Overloading     (== for Move/Piece comparison)
//  6. Constructors/Destructors (all classes)
//  7. STL Containers           (vector, stack)
//  8. Exception Handling       (InvalidMoveException)
//  9. Lambdas                  (inside move generation)
//  10. Minimax AI              (with Alpha-Beta Pruning)
// ============================================================

#include "raylib.h"
#include <vector>
#include <string>
#include <stack>
#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <atomic>

// ─── Constants ───────────────────────────────────────────────
const int CELL = 80;
const int BOARD = 8 * CELL;
const int PANEL = 240;
const int SCR_W = BOARD + PANEL;
const int SCR_H = BOARD;
const int BOT_DEPTH = 3;

// ─── Colors ──────────────────────────────────────────────────
const Color TILE_L = {240, 217, 181, 255};
const Color TILE_D = {181, 136, 99, 255};
const Color C_SEL = {80, 200, 80, 160};
const Color C_HINT = {100, 180, 255, 130};
const Color C_CHECK = {255, 50, 50, 160};
const Color C_PANEL = {28, 28, 38, 255};
const Color C_BOT = {255, 180, 50, 255};

Font gFont;

// ─── Enums ───────────────────────────────────────────────────
enum PieceType
{
    PT_NONE = 0,
    PT_PAWN,
    PT_KNIGHT,
    PT_BISHOP,
    PT_ROOK,
    PT_QUEEN,
    PT_KING
};
enum Side
{
    SIDE_W = 0,
    SIDE_B = 1
};

// ─── Unicode symbols ─────────────────────────────────────────
static const int CP_W[7] = {0, 0x2659, 0x2658, 0x2657, 0x2656, 0x2655, 0x2654};
static const int CP_B[7] = {0, 0x265F, 0x265E, 0x265D, 0x265C, 0x265B, 0x265A};

static std::string toUtf8(int cp)
{
    std::string s;
    if (cp < 0x80)
    {
        s += (char)cp;
    }
    else if (cp < 0x800)
    {
        s += (char)(0xC0 | (cp >> 6));
        s += (char)(0x80 | (cp & 0x3F));
    }
    else
    {
        s += (char)(0xE0 | (cp >> 12));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    }
    return s;
}

static void drawGlyph(PieceType t, Side s, int cx, int cy, int sz = 56)
{
    if (t == PT_NONE)
        return;
    std::string txt = toUtf8((s == SIDE_W) ? CP_W[t] : CP_B[t]);
    DrawTextEx(gFont, txt.c_str(), {(float)(cx - sz / 2 + 2 + 6), (float)(cy - sz / 2 + 2)}, sz, 0, {0, 0, 0, 100});
    Color ol = (s == SIDE_W) ? Color{60, 40, 20, 255} : Color{220, 200, 160, 255};
    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
        {
            if (!dx && !dy)
                continue;
            DrawTextEx(gFont, txt.c_str(), {(float)(cx - sz / 2 + dx + 6), (float)(cy - sz / 2 + dy)}, sz, 0, ol);
        }
    Color fl = (s == SIDE_W) ? Color{255, 252, 230, 255} : Color{35, 22, 10, 255};
    DrawTextEx(gFont, txt.c_str(), {(float)(cx - sz / 2 + 6), (float)(cy - sz / 2)}, sz, 0, fl);
}

// ============================================================
//  EXCEPTION CLASS
// ============================================================
class InvalidMoveException : public std::exception
{
private:
    std::string message;

public:
    InvalidMoveException(const std::string &msg) : message(msg) {}
    const char *what() const noexcept override { return message.c_str(); }
};

// ============================================================
//  MOVE STRUCT
// ============================================================
struct Move
{
    int fr, fc, tr, tc;
    bool enPassant, castleKing, castleQueen;

    Move(int fr = 0, int fc = 0, int tr = 0, int tc = 0,
         bool ep = false, bool ck = false, bool cq = false)
        : fr(fr), fc(fc), tr(tr), tc(tc),
          enPassant(ep), castleKing(ck), castleQueen(cq) {}

    // OPERATOR OVERLOADING
    bool operator==(const Move &o) const
    {
        return fr == o.fr && fc == o.fc && tr == o.tr && tc == o.tc;
    }
};

// ============================================================
//  SNAPSHOT
// ============================================================
struct Snapshot
{
    // Har cell ke liye: type aur side store karo
    PieceType types[8][8];
    Side sides[8][8];
    bool moved[8][8];
    int epCol;
    Side epSide;

    Snapshot()
    {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
            {
                types[r][c] = PT_NONE;
                sides[r][c] = SIDE_W;
                moved[r][c] = false;
            }
        epCol = -1;
        epSide = SIDE_W;
    }
};

// ============================================================
//  ABSTRACT BASE CLASS: Piece
// ============================================================
class Piece
{
protected:
    PieceType type;
    Side side;
    bool moved;

public:
    Piece(PieceType t, Side s) : type(t), side(s), moved(false) {}
    virtual ~Piece() {}

    // Getters
    PieceType getType() const { return type; }
    Side getSide() const { return side; }
    bool hasMoved() const { return moved; }
    bool isEmpty() const { return type == PT_NONE; }

    // Setters
    void setMoved(bool m) { moved = m; }
    void setType(PieceType t) { type = t; }

    // OPERATOR OVERLOADING
    bool operator==(const Piece &o) const
    {
        return type == o.type && side == o.side;
    }
    bool operator!=(const Piece &o) const { return !(*this == o); }

    // PURE VIRTUAL — abstract class
    virtual std::vector<Move> getValidMoves(
        int r, int c,
        const Piece *grid[8][8],
        int epCol) const = 0;

    virtual void draw(int cx, int cy, int sz = 56) const
    {
        drawGlyph(type, side, cx, cy, sz);
    }

    virtual Piece *clone() const = 0;
};

// ============================================================
//  DERIVED CLASSES — Each piece inherits from Piece
// ============================================================

// ── Pawn ─────────────────────────────────────────────────────
class Pawn : public Piece
{
public:
    Pawn(Side s) : Piece(PT_PAWN, s) {}
    ~Pawn() override {}
    Piece *clone() const override { return new Pawn(*this); }

    std::vector<Move> getValidMoves(int r, int c,
                                    const Piece *grid[8][8], int epCol) const override
    {
        std::vector<Move> mv;
        auto inB = [](int r, int c)
        { return r >= 0 && r < 8 && c >= 0 && c < 8; };
        int dir = (side == SIDE_W) ? -1 : 1;
        int start = (side == SIDE_W) ? 6 : 1;
        Side them = (side == SIDE_W) ? SIDE_B : SIDE_W;

        // Forward
        if (inB(r + dir, c) && grid[r + dir][c]->isEmpty())
        {
            mv.push_back(Move(r, c, r + dir, c));
            if (r == start && grid[r + 2 * dir][c]->isEmpty())
                mv.push_back(Move(r, c, r + 2 * dir, c));
        }
        // Diagonal capture + en-passant
        for (int dc : {-1, 1})
        {
            int nr = r + dir, nc = c + dc;
            if (!inB(nr, nc))
                continue;
            if (!grid[nr][nc]->isEmpty() && grid[nr][nc]->getSide() == them)
                mv.push_back(Move(r, c, nr, nc));
            if (epCol == nc && !grid[r][nc]->isEmpty() &&
                grid[r][nc]->getSide() == them &&
                grid[r][nc]->getType() == PT_PAWN)
                mv.push_back(Move(r, c, nr, nc, true));
        }
        return mv;
    }
};

// ── Knight ───────────────────────────────────────────────────
class Knight : public Piece
{
public:
    Knight(Side s) : Piece(PT_KNIGHT, s) {}
    ~Knight() override {}
    Piece *clone() const override { return new Knight(*this); }

    std::vector<Move> getValidMoves(int r, int c,
                                    const Piece *grid[8][8], int) const override
    {
        std::vector<Move> mv;
        auto inB = [](int r, int c)
        { return r >= 0 && r < 8 && c >= 0 && c < 8; };
        Side them = (side == SIDE_W) ? SIDE_B : SIDE_W;
        int dx[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
        int dy[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
        for (int i = 0; i < 8; i++)
        {
            int nr = r + dx[i], nc = c + dy[i];
            if (inB(nr, nc) && (grid[nr][nc]->isEmpty() ||
                                grid[nr][nc]->getSide() == them))
                mv.push_back(Move(r, c, nr, nc));
        }
        return mv;
    }
};

// ── Bishop ───────────────────────────────────────────────────
class Bishop : public Piece
{
public:
    Bishop(Side s) : Piece(PT_BISHOP, s) {}
    ~Bishop() override {}
    Piece *clone() const override { return new Bishop(*this); }

    std::vector<Move> getValidMoves(int r, int c,
                                    const Piece *grid[8][8], int) const override
    {
        std::vector<Move> mv;
        auto inB = [](int r, int c)
        { return r >= 0 && r < 8 && c >= 0 && c < 8; };
        Side them = (side == SIDE_W) ? SIDE_B : SIDE_W;
        auto slide = [&](int dr, int dc)
        {
            int nr = r + dr, nc = c + dc;
            while (inB(nr, nc))
            {
                if (grid[nr][nc]->isEmpty())
                    mv.push_back(Move(r, c, nr, nc));
                else
                {
                    if (grid[nr][nc]->getSide() == them)
                        mv.push_back(Move(r, c, nr, nc));
                    break;
                }
                nr += dr;
                nc += dc;
            }
        };
        slide(-1, -1);
        slide(-1, 1);
        slide(1, -1);
        slide(1, 1);
        return mv;
    }
};

// ── Rook ─────────────────────────────────────────────────────
class Rook : public Piece
{
public:
    Rook(Side s) : Piece(PT_ROOK, s) {}
    ~Rook() override {}
    Piece *clone() const override { return new Rook(*this); }

    std::vector<Move> getValidMoves(int r, int c,
                                    const Piece *grid[8][8], int) const override
    {
        std::vector<Move> mv;
        auto inB = [](int r, int c)
        { return r >= 0 && r < 8 && c >= 0 && c < 8; };
        Side them = (side == SIDE_W) ? SIDE_B : SIDE_W;
        auto slide = [&](int dr, int dc)
        {
            int nr = r + dr, nc = c + dc;
            while (inB(nr, nc))
            {
                if (grid[nr][nc]->isEmpty())
                    mv.push_back(Move(r, c, nr, nc));
                else
                {
                    if (grid[nr][nc]->getSide() == them)
                        mv.push_back(Move(r, c, nr, nc));
                    break;
                }
                nr += dr;
                nc += dc;
            }
        };
        slide(-1, 0);
        slide(1, 0);
        slide(0, -1);
        slide(0, 1);
        return mv;
    }
};

// ── Queen ────────────────────────────────────────────────────
class Queen : public Piece
{
public:
    Queen(Side s) : Piece(PT_QUEEN, s) {}
    ~Queen() override {}
    Piece *clone() const override { return new Queen(*this); }

    std::vector<Move> getValidMoves(int r, int c,
                                    const Piece *grid[8][8], int) const override
    {
        std::vector<Move> mv;
        auto inB = [](int r, int c)
        { return r >= 0 && r < 8 && c >= 0 && c < 8; };
        Side them = (side == SIDE_W) ? SIDE_B : SIDE_W;
        auto slide = [&](int dr, int dc)
        {
            int nr = r + dr, nc = c + dc;
            while (inB(nr, nc))
            {
                if (grid[nr][nc]->isEmpty())
                    mv.push_back(Move(r, c, nr, nc));
                else
                {
                    if (grid[nr][nc]->getSide() == them)
                        mv.push_back(Move(r, c, nr, nc));
                    break;
                }
                nr += dr;
                nc += dc;
            }
        };
        slide(-1, -1);
        slide(-1, 1);
        slide(1, -1);
        slide(1, 1);
        slide(-1, 0);
        slide(1, 0);
        slide(0, -1);
        slide(0, 1);
        return mv;
    }
};

// ── King ─────────────────────────────────────────────────────
class King : public Piece
{
public:
    King(Side s) : Piece(PT_KING, s) {}
    ~King() override {}
    Piece *clone() const override { return new King(*this); }

    std::vector<Move> getValidMoves(int r, int c,
                                    const Piece *grid[8][8], int) const override
    {
        std::vector<Move> mv;
        auto inB = [](int r, int c)
        { return r >= 0 && r < 8 && c >= 0 && c < 8; };
        Side them = (side == SIDE_W) ? SIDE_B : SIDE_W;
        for (int dr = -1; dr <= 1; dr++)
            for (int dc = -1; dc <= 1; dc++)
            {
                if (!dr && !dc)
                    continue;
                int nr = r + dr, nc = c + dc;
                if (inB(nr, nc) && (grid[nr][nc]->isEmpty() ||
                                    grid[nr][nc]->getSide() == them))
                    mv.push_back(Move(r, c, nr, nc));
            }
        // Castling
        if (!moved)
        {
            int row = (side == SIDE_W) ? 7 : 0;
            if (grid[row][5]->isEmpty() && grid[row][6]->isEmpty() &&
                grid[row][7]->getType() == PT_ROOK && !grid[row][7]->hasMoved())
                mv.push_back(Move(r, c, row, 6, false, true, false));
            if (grid[row][3]->isEmpty() && grid[row][2]->isEmpty() &&
                grid[row][1]->isEmpty() &&
                grid[row][0]->getType() == PT_ROOK && !grid[row][0]->hasMoved())
                mv.push_back(Move(r, c, row, 2, false, false, true));
        }
        return mv;
    }
};

// ── EmptyPiece ───────────────────────────────────────────────
class EmptyPiece : public Piece
{
public:
    EmptyPiece() : Piece(PT_NONE, SIDE_W) {}
    ~EmptyPiece() override {}
    Piece *clone() const override { return new EmptyPiece(); }
    void draw(int, int, int) const override {}
    std::vector<Move> getValidMoves(int, int,
                                    const Piece *[8][8], int) const override { return {}; }
};

// ── Helper: make a piece from type+side ──────────────────────
static Piece *makePiece(PieceType t, Side s)
{
    switch (t)
    {
    case PT_PAWN:
        return new Pawn(s);
    case PT_KNIGHT:
        return new Knight(s);
    case PT_BISHOP:
        return new Bishop(s);
    case PT_ROOK:
        return new Rook(s);
    case PT_QUEEN:
        return new Queen(s);
    case PT_KING:
        return new King(s);
    default:
        return new EmptyPiece();
    }
}

// ============================================================
//  BOARD CLASS
// ============================================================
class Board
{
private:
    Piece *grid[8][8];
    int epCol;
    Side epSide;
    bool wCheck, bCheck;

    void setCell(int r, int c, Piece *p)
    {
        delete grid[r][c];
        grid[r][c] = p;
    }

    // Build const grid for passing to getValidMoves
    void buildConstGrid(const Piece *g[8][8]) const
    {
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                g[i][j] = grid[i][j];
    }

public:
    // ── Constructor ──────────────────────────────────────────
    Board() : epCol(-1), epSide(SIDE_W), wCheck(false), bCheck(false)
    {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                grid[r][c] = new EmptyPiece();
        setup();
    }

    // ── Copy Constructor ─────────────────────────────────────
    Board(const Board &o) : epCol(o.epCol), epSide(o.epSide),
                            wCheck(o.wCheck), bCheck(o.bCheck)
    {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                grid[r][c] = o.grid[r][c]->clone();
    }

    // ── Assignment Operator ──────────────────────────────────
    Board &operator=(const Board &o)
    {
        if (this == &o)
            return *this;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                delete grid[r][c];
        epCol = o.epCol;
        epSide = o.epSide;
        wCheck = o.wCheck;
        bCheck = o.bCheck;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                grid[r][c] = o.grid[r][c]->clone();
        return *this;
    }

    // ── Destructor ───────────────────────────────────────────
    ~Board()
    {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                delete grid[r][c];
    }

    // ── Getters ──────────────────────────────────────────────
    const Piece *getCell(int r, int c) const { return grid[r][c]; }
    bool isWCheck() const { return wCheck; }
    bool isBCheck() const { return bCheck; }
    int getEpCol() const { return epCol; }

    // ── Save to Snapshot ──────────────────────
    Snapshot saveSnapshot() const
    {
        Snapshot s;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
            {
                s.types[r][c] = grid[r][c]->getType();
                s.sides[r][c] = grid[r][c]->getSide();
                s.moved[r][c] = grid[r][c]->hasMoved();
            }
        s.epCol = epCol;
        s.epSide = epSide;
        return s;
    }

    // ── Restore from Snapshot ────────────────────
    void loadSnapshot(const Snapshot &s)
    {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
            {
                Piece *p = makePiece(s.types[r][c], s.sides[r][c]);
                p->setMoved(s.moved[r][c]);
                setCell(r, c, p);
            }
        epCol = s.epCol;
        epSide = s.epSide;
        wCheck = bCheck = false;
    }

    // ── Setup ────────────────────────────────────────────────
    void setup()
    {
        for (int c = 0; c < 8; c++)
        {
            setCell(1, c, new Pawn(SIDE_B));
            setCell(6, c, new Pawn(SIDE_W));
        }
        auto mk = [](int c, Side s) -> Piece *
        {
            switch (c)
            {
            case 0:
            case 7:
                return new Rook(s);
            case 1:
            case 6:
                return new Knight(s);
            case 2:
            case 5:
                return new Bishop(s);
            case 3:
                return new Queen(s);
            case 4:
                return new King(s);
            default:
                return new EmptyPiece();
            }
        };
        for (int c = 0; c < 8; c++)
        {
            setCell(0, c, mk(c, SIDE_B));
            setCell(7, c, mk(c, SIDE_W));
        }
        epCol = -1;
        wCheck = bCheck = false;
    }

    // ── Raw moves for a piece ────────────────────────────────
    std::vector<Move> getRawMoves(int r, int c) const
    {
        const Piece *g[8][8];
        buildConstGrid(g);
        return grid[r][c]->getValidMoves(r, c, g, epCol);
    }

    // ── King in check? ───────────────────────────────────────
    bool kingInCheck(Side side) const
    {
        int kr = -1, kc = -1;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (grid[r][c]->getType() == PT_KING &&
                    grid[r][c]->getSide() == side)
                {
                    kr = r;
                    kc = c;
                }
        if (kr < 0)
            return false;
        Side opp = (side == SIDE_W) ? SIDE_B : SIDE_W;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
            {
                if (grid[r][c]->isEmpty() || grid[r][c]->getSide() != opp)
                    continue;
                auto mv = getRawMoves(r, c);
                for (auto &m : mv)
                    if (m.tr == kr && m.tc == kc)
                        return true;
            }
        return false;
    }

    // ── Legal moves ──────────────────────────────────────────
    std::vector<Move> legalMoves(int r, int c) const
    {
        auto raw = getRawMoves(r, c);
        std::vector<Move> legal;
        Side us = grid[r][c]->getSide();
        for (auto &m : raw)
        {
            Board tmp(*this);
            tmp.applyMove(m);
            if (!tmp.kingInCheck(us))
                legal.push_back(m);
        }
        return legal;
    }

    // ── All legal moves for a side ───────────────────────────
    std::vector<Move> allLegalMoves(Side side) const
    {
        std::vector<Move> all;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (!grid[r][c]->isEmpty() && grid[r][c]->getSide() == side)
                {
                    auto mv = legalMoves(r, c);
                    all.insert(all.end(), mv.begin(), mv.end());
                }
        return all;
    }

    bool hasAnyMove(Side side) const
    {
        return !allLegalMoves(side).empty();
    }

    // ── Apply move ───────────────────────────────────────────
    void applyMove(const Move &m)
    {
        if (m.fr < 0 || m.fr > 7 || m.fc < 0 || m.fc > 7 ||
            m.tr < 0 || m.tr > 7 || m.tc < 0 || m.tc > 7)
            throw InvalidMoveException("Move out of bounds!");
        if (grid[m.fr][m.fc]->isEmpty())
            throw InvalidMoveException("No piece at source!");

        Piece *src = grid[m.fr][m.fc]->clone();
        epCol = -1;

        if (m.enPassant)
        {
            int cr = (src->getSide() == SIDE_W) ? m.tr + 1 : m.tr - 1;
            setCell(cr, m.tc, new EmptyPiece());
        }
        if (m.castleKing)
        {
            Piece *rk = grid[m.tr][7]->clone();
            rk->setMoved(true);
            setCell(m.tr, 5, rk);
            setCell(m.tr, 7, new EmptyPiece());
        }
        if (m.castleQueen)
        {
            Piece *rk = grid[m.tr][0]->clone();
            rk->setMoved(true);
            setCell(m.tr, 3, rk);
            setCell(m.tr, 0, new EmptyPiece());
        }
        if (src->getType() == PT_PAWN && abs(m.tr - m.fr) == 2)
        {
            epCol = m.fc;
            epSide = src->getSide();
        }

        src->setMoved(true);
        setCell(m.tr, m.tc, src);
        setCell(m.fr, m.fc, new EmptyPiece());

        // Pawn promotion
        if (grid[m.tr][m.tc]->getType() == PT_PAWN && (m.tr == 0 || m.tr == 7))
        {
            Side ps = grid[m.tr][m.tc]->getSide();
            setCell(m.tr, m.tc, new Queen(ps));
        }
    }

    void updateChecks()
    {
        wCheck = kingInCheck(SIDE_W);
        bCheck = kingInCheck(SIDE_B);
    }

    // ── Draw tiles ───────────────────────────────────────────
    void drawTiles(int sr, int sc, const std::vector<Move> &hints,
                   int bfr = -1, int bfc = -1, int btr = -1, int btc = -1) const
    {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                DrawRectangle(c * CELL, r * CELL, CELL, CELL,
                              ((r + c) % 2 == 0) ? TILE_L : TILE_D);
        if (bfr >= 0)
        {
            DrawRectangle(bfc * CELL, bfr * CELL, CELL, CELL, {255, 200, 50, 90});
            DrawRectangle(btc * CELL, btr * CELL, CELL, CELL, {255, 200, 50, 90});
        }
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
            {
                auto p = grid[r][c];
                if (p->getType() == PT_KING &&
                    ((p->getSide() == SIDE_W && wCheck) || (p->getSide() == SIDE_B && bCheck)))
                    DrawRectangle(c * CELL, r * CELL, CELL, CELL, C_CHECK);
            }
        if (sr >= 0)
            DrawRectangle(sc * CELL, sr * CELL, CELL, CELL, C_SEL);
        for (auto &m : hints)
        {
            if (grid[m.tr][m.tc]->isEmpty())
                DrawCircle(m.tc * CELL + CELL / 2, m.tr * CELL + CELL / 2, 10, C_HINT);
            else
                DrawRectangle(m.tc * CELL, m.tr * CELL, CELL, CELL, C_HINT);
        }
    }

    // ── Draw pieces ──────────────────────────────────────────
    void drawPieces() const
    {
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                grid[r][c]->draw(c * CELL + CELL / 2, r * CELL + CELL / 2);
    }
};

// ============================================================
//  AI CLASS — Minimax + Alpha-Beta Pruning
// ============================================================
class AI
{
private:
    static const int VAL[7];
    static const int PST_PAWN[8][8];

    int evaluate(const Board &b) const
    {
        int score = 0;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
            {
                auto p = b.getCell(r, c);
                if (p->isEmpty())
                    continue;
                int v = VAL[p->getType()];
                if (p->getType() == PT_PAWN)
                {
                    int row = (p->getSide() == SIDE_W) ? r : (7 - r);
                    v += PST_PAWN[row][c];
                }
                score += (p->getSide() == SIDE_B) ? v : -v;
            }
        return score;
    }

    int minimax(Board b, int depth, int alpha, int beta, bool maximizing)
    {
        if (depth == 0)
            return evaluate(b);
        Side side = maximizing ? SIDE_B : SIDE_W;
        auto moves = b.allLegalMoves(side);
        if (moves.empty())
        {
            if (b.kingInCheck(side))
                return maximizing ? -99999 : 99999;
            return 0;
        }
        if (maximizing)
        {
            int best = -99999;
            for (auto &m : moves)
            {
                Board tmp(b);
                tmp.applyMove(m);
                best = std::max(best, minimax(tmp, depth - 1, alpha, beta, false));
                alpha = std::max(alpha, best);
                if (beta <= alpha)
                    break;
            }
            return best;
        }
        else
        {
            int best = 99999;
            for (auto &m : moves)
            {
                Board tmp(b);
                tmp.applyMove(m);
                best = std::min(best, minimax(tmp, depth - 1, alpha, beta, true));
                beta = std::min(beta, best);
                if (beta <= alpha)
                    break;
            }
            return best;
        }
    }

public:
    Move findBestMove(const Board &b)
    {
        auto moves = b.allLegalMoves(SIDE_B);
        if (moves.empty())
            return Move();
        int best = -99999;
        Move bm = moves[0];
        for (auto &m : moves)
        {
            Board tmp(b);
            tmp.applyMove(m);
            int val = minimax(tmp, BOT_DEPTH - 1, -99999, 99999, false);
            if (val > best)
            {
                best = val;
                bm = m;
            }
        }
        return bm;
    }
};

const int AI::VAL[7] = {0, 100, 320, 330, 500, 900, 20000};
const int AI::PST_PAWN[8][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {50, 50, 50, 50, 50, 50, 50, 50},
    {10, 10, 20, 30, 30, 20, 10, 10},
    {5, 5, 10, 25, 25, 10, 5, 5},
    {0, 0, 0, 20, 20, 0, 0, 0},
    {5, -5, -10, 0, 0, -10, -5, 5},
    {5, 10, 10, -20, -20, 10, 10, 5},
    {0, 0, 0, 0, 0, 0, 0, 0}};

// ============================================================
//  GAME CLASS
// ============================================================
class Game
{
private:
    Board board;
    Side turn;
    int selR, selC;
    std::vector<Move> hints;
    bool gameOver;
    std::string status;
    std::string errorMsg;

    // STL STACK
    std::stack<Snapshot> history;

    int botFr, botFc, botTr, botTc;
    AI ai;

    // ── Thread variables ─────────────────────────────────────
    std::thread botThread;        // bot alag thread mein chalega
    std::atomic<bool> botRunning; // bot abhi soch raha hai?
    std::atomic<bool> botDone;    // bot ne move decide kar liya?
    Move botResult;               // bot ka chosen move

public:
    Game() : turn(SIDE_W), selR(-1), selC(-1),
             gameOver(false), status("Your Turn (White)"),
             botFr(-1), botFc(-1), botTr(-1), botTc(-1),
             botRunning(false), botDone(false), botResult(Move()) {}

    ~Game()
    {
        // Destructor
        if (botThread.joinable())
            botThread.join();
    }

    void reset()
    {
        // Pehle thread band karo
        if (botThread.joinable())
            botThread.join();
        botRunning = false;
        botDone = false;

        board = Board();
        turn = SIDE_W;
        selR = selC = -1;
        hints.clear();
        gameOver = false;
        status = "Your Turn (White)";
        errorMsg = "";
        botFr = botFc = botTr = botTc = -1;
        while (!history.empty())
            history.pop();
    }

    void updateStatus()
    {
        board.updateChecks();
        bool any = board.hasAnyMove(turn);
        if (!any)
        {
            gameOver = true;
            bool inCk = (turn == SIDE_W) ? board.isWCheck() : board.isBCheck();
            status = inCk ? ((turn == SIDE_W) ? "Bot Wins! Checkmate" : "You Win! Checkmate!")
                          : "Stalemate! Draw";
        }
        else
        {
            if (turn == SIDE_W)
                status = board.isWCheck() ? "You're in Check!" : "Your Turn (White)";
            else
                status = board.isBCheck() ? "Bot in Check!" : "Bot Thinking...";
        }
    }

    void handleClick(int mx, int my)
    {
        if (gameOver || turn != SIDE_W)
            return;
        int c = mx / CELL, r = my / CELL;
        if (c < 0 || c > 7 || r < 0 || r > 7)
            return;

        if (selR >= 0)
        {
            for (auto &m : hints)
            {
                if (m.tr == r && m.tc == c)
                {
                    try
                    {
                        // Save snapshot BEFORE move
                        history.push(board.saveSnapshot());
                        board.applyMove(m);
                        turn = SIDE_B;
                        selR = selC = -1;
                        hints.clear();
                        errorMsg = "";
                        updateStatus();
                    }
                    catch (InvalidMoveException &e)
                    {
                        errorMsg = e.what();
                    }
                    return;
                }
            }
        }
        if (!board.getCell(r, c)->isEmpty() &&
            board.getCell(r, c)->getSide() == SIDE_W)
        {
            selR = r;
            selC = c;
            hints = board.legalMoves(r, c);
        }
        else
        {
            selR = selC = -1;
            hints.clear();
        }
    }

    // UNDO
    void undoMove()
    {
        // 2 snapshots chahiye: player + bot move
        if (history.size() >= 2)
        {
            history.pop();              // bot ka snapshot hata do
            Snapshot s = history.top(); // player ka snapshot lo
            history.pop();
            board.loadSnapshot(s); // board restore karo
            turn = SIDE_W;
            selR = selC = -1;
            hints.clear();
            gameOver = false;
            botFr = botFc = botTr = botTc = -1;
            errorMsg = "";
            updateStatus();
        }
        else
        {
            errorMsg = "Nothing to undo!";
        }
    }

    // Bot move
    void botMove()
    {
        if (gameOver || turn != SIDE_B)
            return;

        if (botRunning)
        {
            if (botDone)
            {
                if (botThread.joinable())
                    botThread.join();
                botRunning = false;
                botDone = false;

                Move bm = botResult;
                botFr = bm.fr;
                botFc = bm.fc;
                botTr = bm.tr;
                botTc = bm.tc;
                try
                {
                    history.push(board.saveSnapshot());
                    board.applyMove(bm);
                }
                catch (InvalidMoveException &e)
                {
                    errorMsg = e.what();
                }
                turn = SIDE_W;
                updateStatus();
            }
            return;
        }

        if (!board.hasAnyMove(SIDE_B))
            return;

        botRunning = true;
        botDone = false;
        Board boardCopy = board;

        botThread = std::thread([this, boardCopy]() mutable
                                {
                                    botResult = ai.findBestMove(boardCopy);
                                    botDone = true; });
    }

    void draw()
    {
        board.drawTiles(selR, selC, hints, botFr, botFc, botTr, botTc);
        board.drawPieces();

        // Coordinates
        for (int i = 0; i < 8; i++)
        {
            char b[2] = {char('a' + i), 0};
            DrawText(b, i * CELL + 5, BOARD - 18, 14, (i % 2 == 0) ? TILE_D : TILE_L);
            char b2[2] = {char('8' - i), 0};
            DrawText(b2, 5, i * CELL + 5, 14, (i % 2 == 0) ? TILE_D : TILE_L);
        }

        // Panel
        DrawRectangle(BOARD, 0, PANEL, SCR_H, C_PANEL);
        DrawText("CHESS", BOARD + 30, 14, 32, RAYWHITE);
        DrawText("vs Bot", BOARD + 36, 48, 16, C_BOT);
        DrawLine(BOARD + 12, 70, BOARD + PANEL - 12, 70, {70, 70, 90, 255});

        drawGlyph(PT_KING, turn, BOARD + 28, 94, 36);
        Color tc = (turn == SIDE_W) ? Color{255, 252, 230, 255} : C_BOT;
        DrawText((turn == SIDE_W) ? "YOUR TURN" : "BOT...", BOARD + 52, 82, 16, tc);

        DrawLine(BOARD + 12, 112, BOARD + PANEL - 12, 112, {70, 70, 90, 255});
        DrawText(status.c_str(), BOARD + 12, 118, 13, YELLOW);
        if (!errorMsg.empty())
            DrawText(errorMsg.c_str(), BOARD + 12, 136, 11, RED);

        DrawLine(BOARD + 12, 150, BOARD + PANEL - 12, 150, {70, 70, 90, 255});

        DrawText("YOU", BOARD + 16, 158, 15, {200, 230, 255, 255});
        DrawText("White", BOARD + 16, 175, 12, LIGHTGRAY);
        drawGlyph(PT_KING, SIDE_W, BOARD + 85, 166, 26);
        DrawText("BOT", BOARD + 115, 158, 15, C_BOT);
        DrawText("Black", BOARD + 115, 175, 12, LIGHTGRAY);
        drawGlyph(PT_KING, SIDE_B, BOARD + 185, 166, 26);

        DrawLine(BOARD + 12, 196, BOARD + PANEL - 12, 196, {70, 70, 90, 255});

        const char *nm[6] = {"Pawn", "Rook", "Knight", "Bishop", "Queen", "King"};
        PieceType tp[6] = {PT_PAWN, PT_ROOK, PT_KNIGHT, PT_BISHOP, PT_QUEEN, PT_KING};
        for (int i = 0; i < 6; i++)
        {
            int ly = 212 + i * 34;
            drawGlyph(tp[i], SIDE_W, BOARD + 18, ly, 24);
            drawGlyph(tp[i], SIDE_B, BOARD + 44, ly, 24);
            DrawText(nm[i], BOARD + 70, ly - 8, 13, LIGHTGRAY);
        }

        DrawLine(BOARD + 12, 420, BOARD + PANEL - 12, 420, {70, 70, 90, 255});
        DrawText("Click = select/move", BOARD + 12, 428, 12, {140, 140, 160, 255});
        DrawText("U = Undo move", BOARD + 12, 444, 12, {140, 140, 160, 255});
        DrawText("R = Restart", BOARD + 12, 460, 12, {140, 140, 160, 255});

        char ub[40];
        sprintf(ub, "Moves: %d", (int)history.size());
        DrawText(ub, BOARD + 12, 476, 12, {100, 100, 120, 255});
        char db[40];
        sprintf(db, "Bot Depth: %d", BOT_DEPTH);
        DrawText(db, BOARD + 12, 492, 12, {100, 100, 120, 255});

        if (gameOver)
        {
            DrawRectangle(0, SCR_H / 2 - 55, BOARD, 110, {0, 0, 0, 210});
            DrawText(status.c_str(), 30, SCR_H / 2 - 34, 26, RED);
            DrawText("R=Restart  U=Undo", 30, SCR_H / 2 + 12, 20, RAYWHITE);
        }
    }
};

// ============================================================
//  MAIN
// ============================================================
int main()
{
    InitWindow(SCR_W, SCR_H, "Chess vs Bot - OOP Project");
    SetTargetFPS(60);

    int codepoints[12];
    for (int i = 0; i < 6; i++)
    {
        codepoints[i] = 0x2654 + i;
        codepoints[i + 6] = 0x265A + i;
    }
    gFont = LoadFontEx("C:\\Windows\\Fonts\\seguisym.ttf", 72, codepoints, 12);
    if (gFont.texture.id == 0)
        gFont = LoadFontEx("C:\\Windows\\Fonts\\arial.ttf", 72, codepoints, 12);

    Game game;

    while (!WindowShouldClose())
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            int mx = GetMouseX(), my = GetMouseY();
            if (mx < BOARD)
                game.handleClick(mx, my);
        }
        if (IsKeyPressed(KEY_R))
            game.reset();
        if (IsKeyPressed(KEY_U))
            game.undoMove();

        game.botMove();

        BeginDrawing();
        ClearBackground({18, 18, 26, 255});
        game.draw();
        EndDrawing();
    }

    UnloadFont(gFont);
    CloseWindow();
    return 0;
}