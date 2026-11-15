// RollUi.hpp: My lil Roll's editor: the note grid, the velocity lane and the tool strip. UI thread only.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include "Roll.hpp"
#include "Tools.h"
#include <climits>
#include <functional>

static const float ROLL_KEYW = 16.f;   // px of piano keys at the grid's left
inline double clampd (double x, double a, double b) { return std::max (a, std::min (x, b)); }
inline NVGcolor rollCol (int r, int g, int b, int a = 255) { return nvgRGBA (r, g, b, a); }

// Shared by the grid, the lane and the strip of one RollWidget. Edits m->doc, then commit() sends it to the audio thread.
struct RollEditor
{
    enum Tool { DRAW, SELECT, ERASE, TOOL_N };
    Roll* m = nullptr;
    int tool = DRAW;
    double t0 = 0.0, ppt = 1.0;   // tick at the grid's left edge, pixels per tick
    int top = 84;                 // pitch of the top row (C6)
    float rowH = 6.f;
    std::vector<char> sel;        // parallel to the notes
    History hist;
    std::vector<Note> clip;
    int lastLen = PPQ;            // new notes take the length of the last one touched (FL)
    int seenGen = -1;             // Roll::docGen this editor state belongs to
    bool previewing = false;      // a TOOLS submenu is previewing on the notes: the grid takes no keys
    int cur = 0;                  // the pattern shown and edited: follows Roll::selected

    Pattern& pat() { return m->doc.pats[cur]; }
    std::vector<Note>& notes() { return pat().notes; }

    // Follows a doc reload and the PAT knob/CV. The edited pattern never changes under a held mouse button or an
    // open tool preview; it catches up on the next call after. The clipboard survives a pattern change (copy between patterns).
    void syncSel()
    {
        if (! m) return;
        const int want = m->selected.load();
        const bool held = glfwGetMouseButton (APP->window->win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS
            || glfwGetMouseButton (APP->window->win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        if (m->docGen != seenGen) { seenGen = m->docGen; cur = want; hist = History(); clip.clear(); sel.assign (notes().size(), 0); }
        else if (want != cur && ! previewing && ! held) { cur = want; hist = History(); sel.assign (notes().size(), 0); }
        sel.resize (notes().size(), 0);
    }
    bool anySel() const { for (char s : sel) if (s) return true; return false; }
    void selectNone() { std::fill (sel.begin(), sel.end(), 0); }
    int snap() const { return SNAPS[m->doc.snap].ticks; }
    int loopTicks() const { return m->doc.pats[cur].ticks(); }
    ToolCtx toolCtx() const { ToolCtx c; c.loopTicks = loopTicks(); c.snap = snap(); c.key = m->doc.key; c.mode = m->doc.mode; return c; }
    void commit() { sel.resize (notes().size(), 0); m->commit (cur); }   // never switches pattern mid-edit
    void record (const std::vector<Note>& before) { if (before != notes()) hist.push (before); }   // one undo step per real change
    void undo() { if (hist.back (notes())) { sel.assign (notes().size(), 0); commit(); } }
    void redo() { if (hist.forward (notes())) { sel.assign (notes().size(), 0); commit(); } }
    void eraseIndex (int i) { notes().erase (notes().begin() + i); sel.erase (sel.begin() + i); }

    // After a move: notes on the same start + pitch keep one, a moved note if any (it replaces what it landed on, FL),
    // the highest index among equals. moved[i] = note i was just moved; it may be sel itself, so it is read before compacting.
    // ponytail: O(n^2) over up to MAX_NOTES, but only on mouse-up and key presses
    void dedupe (const std::vector<char>& moved)
    {
        syncSel();
        std::vector<Note>& n = notes();
        std::vector<char> mv (n.size(), 0), dead (n.size(), 0);
        for (int i = 0; i < (int) n.size() && i < (int) moved.size(); ++i) mv[i] = moved[i];
        for (int i = 0; i < (int) n.size(); ++i)
            for (int j = 0; j < (int) n.size(); ++j)
                if (j != i && n[j].start == n[i].start && n[j].pitch == n[i].pitch && (mv[j] > mv[i] || (mv[j] == mv[i] && j > i)))
                {
                    dead[i] = 1;   // j beats i: moved over unmoved, else the higher index
                    break;
                }
        int k = 0;
        for (int i = 0; i < (int) n.size(); ++i)
            if (! dead[i]) { n[k] = n[i]; sel[k] = sel[i]; ++k; }
        n.resize (k);
        sel.resize (k);
    }
};

struct RollGrid : OpaqueWidget
{
    enum Drag { NONE, MOVE, RESIZE, BOX, ERASING };
    RollEditor* ed = nullptr;
    int drag = NONE;
    Vec down, cur;                    // local px
    std::vector<Note> undoBase;       // notes at press: the undo step
    std::vector<Note> origin;         // notes after the press (incl. a fresh stamp): move/resize math
    std::vector<int> grabbed;         // indices being moved / resized

    double tickAt (float x) const { return ed->t0 + (x - ROLL_KEYW) / ed->ppt; }
    int pitchAt (float y) const { return ed->top - (int) std::floor (y / ed->rowH); }
    float xOf (double t) const { return ROLL_KEYW + (float) ((t - ed->t0) * ed->ppt); }
    float yOf (int p) const { return (ed->top - p) * ed->rowH; }
    int rows() const { return (int) std::ceil (box.size.y / ed->rowH); }

    int hit (Vec p)   // the topmost note under p, or -1
    {
        std::vector<Note>& n = ed->notes();
        const int pi = pitchAt (p.y);
        const double t = tickAt (p.x);
        for (int i = (int) n.size() - 1; i >= 0; --i)
            if (n[i].pitch == pi && t >= n[i].start && t < n[i].start + n[i].len) return i;
        return -1;
    }
    bool nearEnd (int i, Vec p)   // the right edge resizes; capped at a third of the note so a short one can still be moved
    {
        const Note& n = ed->notes()[i];
        return xOf (n.start + n.len) - p.x < std::min (5.f, (xOf (n.start + n.len) - xOf (n.start)) / 3.f);
    }

    void grab (int i)   // the note, or the whole selection when the note is part of it
    {
        grabbed.clear();
        if (ed->sel[i]) { for (int j = 0; j < (int) ed->sel.size(); ++j) if (ed->sel[j]) grabbed.push_back (j); }
        else grabbed.push_back (i);
    }

    void stampAt (Vec p)   // one click: a note, or the whole chord with the clicked row as root
    {
        RollDoc& d = ed->m->doc;
        const int s = ed->snap(), root = pitchAt (p.y);
        const int start = (int) std::floor (tickAt (p.x) / s) * s;
        grabbed.clear();
        if (root < 0 || root > 127 || start < 0 || start >= ed->loopTicks()) return;
        for (int pitch : stamp (root, d.chord, d.key, d.mode, d.scaleSnap))
        {
            if (! ed->pat().add (Note (start, ed->lastLen, pitch, 0.8f))) break;
            ed->syncSel();
            for (int i = 0; i < (int) ed->notes().size(); ++i)
                if (ed->notes()[i].start == start && ed->notes()[i].pitch == pitch) grabbed.push_back (i);
        }
        ed->commit();
    }

    void eraseAt (Vec p)
    {
        const int i = hit (p);
        if (i >= 0) { ed->eraseIndex (i); ed->commit(); }
    }

    void clampView()
    {
        const double vis = (box.size.x - ROLL_KEYW) / ed->ppt;
        ed->t0 = clampd (ed->t0, 0.0, std::max (0.0, (double) ed->loopTicks() - vis));
        ed->top = clamp (ed->top, rows() - 1, 127);
    }

    void onButton (const ButtonEvent& e) override
    {
        if (! ed->m || e.action != GLFW_PRESS || (e.button != GLFW_MOUSE_BUTTON_LEFT && e.button != GLFW_MOUSE_BUTTON_RIGHT)) return;
        e.consume (this);
        APP->event->setSelectedWidget (this);   // keys go here now
        ed->syncSel();
        down = cur = e.pos;
        undoBase = ed->notes();
        drag = NONE;
        const bool shift = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
        if (e.pos.x < ROLL_KEYW)
        {
            const int p = pitchAt (e.pos.y);
            if (p >= 0 && p <= 127) ed->m->previewReq = p;
            return;
        }
        const int i = hit (e.pos);
        if (e.button == GLFW_MOUSE_BUTTON_RIGHT || ed->tool == RollEditor::ERASE) { drag = ERASING; eraseAt (e.pos); return; }
        if (ed->tool == RollEditor::SELECT)
        {
            if (i < 0) { if (! shift) ed->selectNone(); drag = BOX; return; }
            if (shift) ed->sel[i] = ! ed->sel[i];
            else if (! ed->sel[i]) { ed->selectNone(); ed->sel[i] = 1; }
        }
        if (i >= 0)
        {
            grab (i);
            ed->lastLen = ed->notes()[i].len;
            drag = nearEnd (i, e.pos) ? RESIZE : MOVE;
        }
        else if (ed->tool == RollEditor::DRAW)
        {
            stampAt (e.pos);
            drag = MOVE;   // drag right after the click moves what was just placed (FL)
        }
        origin = ed->notes();
    }

    void onDragMove (const DragMoveEvent& e) override
    {
        if (! ed->m || drag == NONE) return;
        cur = cur.plus (e.mouseDelta.div (getAbsoluteZoom()));
        if (drag == ERASING) { eraseAt (cur); return; }
        if (drag == BOX) return;
        std::vector<Note>& n = ed->notes();
        const int s = ed->snap();
        const int dt = (int) std::round ((tickAt (cur.x) - tickAt (down.x)) / s) * s;
        for (int i : grabbed)
        {
            if (i >= (int) n.size() || i >= (int) origin.size()) continue;
            if (drag == MOVE)
            {
                n[i].start = clamp (origin[i].start + dt, 0, ed->loopTicks() - 1);
                n[i].pitch = clamp (origin[i].pitch + pitchAt (cur.y) - pitchAt (down.y), 0, 127);
            }
            else ed->lastLen = n[i].len = std::max (s, origin[i].len + dt);
        }
        ed->commit();
    }

    void onDragEnd (const DragEndEvent& e) override
    {
        if (! ed->m) return;
        if (drag == BOX)
        {
            const double ta = tickAt (std::min (down.x, cur.x)), tb = tickAt (std::max (down.x, cur.x));
            const int pa = pitchAt (std::max (down.y, cur.y)), pb = pitchAt (std::min (down.y, cur.y));
            std::vector<Note>& n = ed->notes();
            for (int i = 0; i < (int) n.size(); ++i)
                if (n[i].pitch >= pa && n[i].pitch <= pb && n[i].start < tb && n[i].start + n[i].len > ta) ed->sel[i] = 1;
        }
        if (drag == MOVE)
        {
            std::vector<char> moved (ed->notes().size(), 0);
            for (int i : grabbed) if (i >= 0 && i < (int) moved.size()) moved[i] = 1;
            ed->dedupe (moved);
            ed->commit();
        }
        ed->record (undoBase);
        drag = NONE;
    }

    void nudge (int dt, int dp)
    {
        std::vector<Note>& n = ed->notes();
        for (int i = 0; i < (int) n.size(); ++i)
            if (ed->sel[i])
            {
                n[i].start = clamp (n[i].start + dt, 0, ed->loopTicks() - 1);
                n[i].pitch = clamp (n[i].pitch + dp, 0, 127);
            }
    }

    void copy()
    {
        ed->clip.clear();
        int first = INT_MAX;
        for (int i = 0; i < (int) ed->notes().size(); ++i)
            if (ed->sel[i]) { ed->clip.push_back (ed->notes()[i]); first = std::min (first, ed->notes()[i].start); }
        for (Note& c : ed->clip) c.start -= first;
    }

    void paste()   // at the playhead, snapped down; the pasted notes become the selection
    {
        const int s = ed->snap();
        const int at = (int) std::floor (ed->m->playTick.load() / s) * s;
        ed->selectNone();
        for (Note c : ed->clip)
        {
            c.start += at;
            if (c.start >= ed->loopTicks() || ! ed->pat().add (c)) continue;
            ed->syncSel();
            for (int i = 0; i < (int) ed->notes().size(); ++i)
                if (ed->notes()[i].start == c.start && ed->notes()[i].pitch == c.pitch) ed->sel[i] = 1;
        }
    }

    void onSelectKey (const SelectKeyEvent& e) override
    {
        if (! ed->m || (e.action != GLFW_PRESS && e.action != GLFW_REPEAT)) return;
        if (ed->previewing) { e.consume (this); return; }   // the open tool submenu owns the notes until it closes
        ed->syncSel();
        const int mods = e.mods & RACK_MOD_MASK;
        const bool ctrl = mods == RACK_MOD_CTRL, shift = mods == GLFW_MOD_SHIFT;
        if (ctrl && e.key == GLFW_KEY_Z) { ed->undo(); e.consume (this); return; }
        if ((ctrl && e.key == GLFW_KEY_Y) || (mods == (RACK_MOD_CTRL | GLFW_MOD_SHIFT) && e.key == GLFW_KEY_Z)) { ed->redo(); e.consume (this); return; }
        std::vector<Note>& n = ed->notes();
        const std::vector<Note> base = n;
        const ToolCtx tc = ed->toolCtx();
        if (shift && e.key == GLFW_KEY_Q) quickQuantizeStarts (n, ed->sel, tc);   // never Ctrl+Q: it quits Rack
        else if (ctrl && e.key == GLFW_KEY_U) quickChop (n, ed->sel, tc);
        else if (ctrl && e.key == GLFW_KEY_L) quickLegato (n, ed->sel, tc);
        else if (ctrl && e.key == GLFW_KEY_G) glue (n, ed->sel, tc);
        else if (shift && e.key == GLFW_KEY_D) discardLengths (n, ed->sel, tc);
        else if (mods == (RACK_MOD_CTRL | GLFW_MOD_SHIFT) && (e.key == GLFW_KEY_LEFT || e.key == GLFW_KEY_RIGHT))
            rotate (n, ed->sel, tc, e.key == GLFW_KEY_RIGHT ? 1 : -1);
        else if (ctrl && e.key == GLFW_KEY_A) std::fill (ed->sel.begin(), ed->sel.end(), 1);
        else if (ctrl && e.key == GLFW_KEY_C) copy();
        else if (ctrl && e.key == GLFW_KEY_V) paste();
        else if (mods == 0 && (e.key == GLFW_KEY_DELETE || e.key == GLFW_KEY_BACKSPACE))
        {
            for (int i = (int) n.size() - 1; i >= 0; --i) if (ed->sel[i]) ed->eraseIndex (i);
        }
        else if ((mods == 0 || shift) && (e.key == GLFW_KEY_UP || e.key == GLFW_KEY_DOWN))
        {
            nudge (0, (e.key == GLFW_KEY_UP ? 1 : -1) * (shift ? 12 : 1));
            ed->dedupe (ed->sel);   // nudge moves the selection
        }
        else if (mods == 0 && (e.key == GLFW_KEY_LEFT || e.key == GLFW_KEY_RIGHT))
        {
            nudge ((e.key == GLFW_KEY_RIGHT ? 1 : -1) * ed->snap(), 0);
            ed->dedupe (ed->sel);   // nudge moves the selection
        }
        else return;   // not ours: Rack's own shortcuts still work
        e.consume (this);
        ed->commit();
        ed->record (base);
    }

    void onHoverScroll (const HoverScrollEvent& e) override
    {
        if (! ed->m) return;
        const int mods = APP->window->getMods() & RACK_MOD_MASK;
        const float d = e.scrollDelta.y != 0.f ? e.scrollDelta.y : e.scrollDelta.x;   // Shift+wheel can arrive as x
        if (d == 0.f) return;
        const int dir = d > 0 ? 1 : -1;
        if (mods == RACK_MOD_CTRL)   // zoom time around the mouse
        {
            const double at = tickAt (e.pos.x), vis = (box.size.x - ROLL_KEYW) / ed->ppt;
            const double nv = clampd (vis * (dir > 0 ? 0.8 : 1.25), (double) PPQ, (double) (16 * BAR));
            ed->ppt = (box.size.x - ROLL_KEYW) / nv;
            ed->t0 = at - (e.pos.x - ROLL_KEYW) / ed->ppt;
        }
        else if (mods == GLFW_MOD_SHIFT) ed->t0 -= dir * BAR / 4.0;
        else ed->top += dir * 2;
        clampView();
        e.consume (this);
    }

    void drawLayer (const DrawArgs& args, int layer) override
    {
        if (layer != 1) return;
        if (ed->m) ed->syncSel();   // picks up an Initialize or preset load without input
        NVGcontext* vg = args.vg;
        const float w = box.size.x, h = box.size.y;
        const RollDoc* d = ed->m ? &ed->m->doc : nullptr;
        const int key = d ? d->key : 0, mode = d ? d->mode : 0, L = d ? ed->loopTicks() : 4 * BAR;
        const int s = d ? SNAPS[d->snap].ticks : 24;
        if (ed->m) clampView();
        nvgSave (vg);
        nvgScissor (vg, 0, 0, w, h);
        nvgBeginPath (vg); nvgRect (vg, 0, 0, w, h); nvgFillColor (vg, rollCol (0x16, 0x16, 0x1a)); nvgFill (vg);

        for (int r = 0; r <= rows(); ++r)   // rows: black keys darker, scale rows tinted lime, a line under each C
        {
            const int p = ed->top - r, pc = ((p % 12) + 12) % 12;
            if (p < 0) break;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            NVGcolor c = black ? rollCol (0x12, 0x12, 0x15) : rollCol (0x20, 0x20, 0x27);
            if (inScale (p, key, mode)) c = nvgLerpRGBA (c, rollCol (0xc8, 0xff, 0x3a), 0.08f);
            nvgBeginPath (vg); nvgRect (vg, ROLL_KEYW, r * ed->rowH, w - ROLL_KEYW, ed->rowH); nvgFillColor (vg, c); nvgFill (vg);
            if (pc == 0)
            {
                nvgBeginPath (vg); nvgRect (vg, ROLL_KEYW, (r + 1) * ed->rowH - 0.5f, w - ROLL_KEYW, 0.5f);
                nvgFillColor (vg, rollCol (0x8d, 0x8d, 0x96, 90)); nvgFill (vg);
            }
        }

        const int step = s * ed->ppt >= 6.0 ? s : PPQ * ed->ppt >= 6.0 ? PPQ : BAR;   // drop lines that would be too dense
        for (int t = (int) (std::floor (ed->t0 / step) * step); xOf (t) < w; t += step)
        {
            const int a = t % BAR == 0 ? 140 : t % PPQ == 0 ? 70 : 30;
            nvgBeginPath (vg); nvgRect (vg, xOf (t), 0, t % BAR == 0 ? 1.f : 0.5f, h);
            nvgFillColor (vg, rollCol (0x8d, 0x8d, 0x96, a)); nvgFill (vg);
        }
        if (xOf (L) < w)   // past the loop end
        {
            nvgBeginPath (vg); nvgRect (vg, xOf (L), 0, w - xOf (L), h); nvgFillColor (vg, rollCol (0, 0, 0, 150)); nvgFill (vg);
        }

        if (d)
            for (int i = 0; i < (int) ed->notes().size(); ++i)
            {
                const Note& n = ed->notes()[i];
                const float x0 = xOf (n.start), x1 = xOf (n.start + n.len), y = yOf (n.pitch);
                if (x1 < ROLL_KEYW || x0 > w || y + ed->rowH < 0 || y > h) continue;
                const bool on = i < (int) ed->sel.size() && ed->sel[i];
                nvgBeginPath (vg);
                nvgRoundedRect (vg, std::max (x0, ROLL_KEYW), y + 0.5f, std::max (1.5f, x1 - std::max (x0, ROLL_KEYW) - 0.5f), ed->rowH - 1.f, 1.5f);
                nvgFillColor (vg, nvgLerpRGBA (rollCol (0x24, 0x4a, 0x58), on ? rollCol (0xc8, 0xff, 0x3a) : rollCol (0x5a, 0xc8, 0xe6), 0.35f + 0.65f * n.vel));
                nvgFill (vg);
                if (on) { nvgStrokeColor (vg, rollCol (0xf2, 0xf2, 0xf4)); nvgStrokeWidth (vg, 0.8f); nvgStroke (vg); }
            }

        if (drag == BOX)
        {
            nvgBeginPath (vg);
            nvgRect (vg, std::min (down.x, cur.x), std::min (down.y, cur.y), std::fabs (cur.x - down.x), std::fabs (cur.y - down.y));
            nvgFillColor (vg, rollCol (0xc8, 0xff, 0x3a, 30)); nvgFill (vg);
            nvgStrokeColor (vg, rollCol (0xc8, 0xff, 0x3a)); nvgStrokeWidth (vg, 0.7f); nvgStroke (vg);
        }

        if (ed->m)
        {
            const float x = xOf (ed->m->playTick.load());
            if (x >= ROLL_KEYW && x <= w) { nvgBeginPath (vg); nvgRect (vg, x, 0, 1.2f, h); nvgFillColor (vg, rollCol (0xff, 0x4d, 0x4d)); nvgFill (vg); }
        }

        std::shared_ptr<Font> font = APP->window->loadFont (asset::system ("res/fonts/ShareTechMono-Regular.ttf"));
        for (int r = 0; r <= rows(); ++r)   // keys column
        {
            const int p = ed->top - r, pc = ((p % 12) + 12) % 12;
            if (p < 0) break;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            nvgBeginPath (vg); nvgRect (vg, 0, r * ed->rowH, ROLL_KEYW, ed->rowH - 0.4f);
            nvgFillColor (vg, black ? rollCol (0x16, 0x16, 0x1a) : rollCol (0xe6, 0xe6, 0xea)); nvgFill (vg);
            if (pc == 0 && font && font->handle >= 0)
            {
                nvgFontFaceId (vg, font->handle);
                nvgFontSize (vg, std::min (9.f, ed->rowH + 1.f));
                nvgTextAlign (vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                nvgFillColor (vg, rollCol (0x16, 0x16, 0x1a));
                nvgText (vg, 1.f, (r + 0.5f) * ed->rowH, string::f ("C%d", p / 12 - 1).c_str(), nullptr);
            }
        }
        nvgResetScissor (vg);
        nvgRestore (vg);
    }
};

// Velocity bars under the grid, on the grid's time axis. Dragging draws a line: selected notes, or all when none are.
struct VelLane : OpaqueWidget
{
    RollEditor* ed = nullptr;
    RollGrid* grid = nullptr;
    Vec last;
    std::vector<Note> undoBase;

    void apply (Vec a, Vec b)
    {
        std::vector<Note>& n = ed->notes();
        const bool only = ed->anySel();
        const double tol = 4.0 / ed->ppt;
        double ta = grid->tickAt (a.x), tb = grid->tickAt (b.x);
        float va = 1.f - a.y / box.size.y, vb = 1.f - b.y / box.size.y;
        if (ta > tb) { std::swap (ta, tb); std::swap (va, vb); }
        for (int i = 0; i < (int) n.size(); ++i)
        {
            if ((only && ! ed->sel[i]) || n[i].start < ta - tol || n[i].start > tb + tol) continue;
            const float f = tb - ta > 1e-9 ? (float) clampd ((n[i].start - ta) / (tb - ta), 0.0, 1.0) : 0.f;
            n[i].vel = clamp (va + (vb - va) * f, 0.f, 1.f);
        }
        ed->commit();
    }

    void onButton (const ButtonEvent& e) override
    {
        if (! ed->m || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        e.consume (this);
        ed->syncSel();
        undoBase = ed->notes();
        last = e.pos;
        apply (e.pos, e.pos);
    }
    void onDragMove (const DragMoveEvent& e) override
    {
        if (! ed->m) return;
        const Vec cur = last.plus (e.mouseDelta.div (getAbsoluteZoom()));
        apply (last, cur);
        last = cur;
    }
    void onDragEnd (const DragEndEvent& e) override { if (ed->m) ed->record (undoBase); }

    void drawLayer (const DrawArgs& args, int layer) override
    {
        if (layer != 1) return;
        if (ed->m) ed->syncSel();   // picks up an Initialize or preset load without input
        NVGcontext* vg = args.vg;
        const float w = box.size.x, h = box.size.y;
        nvgSave (vg);
        nvgScissor (vg, 0, 0, w, h);
        nvgBeginPath (vg); nvgRect (vg, 0, 0, w, h); nvgFillColor (vg, rollCol (0x12, 0x12, 0x15)); nvgFill (vg);
        if (ed->m)
            for (int i = 0; i < (int) ed->notes().size(); ++i)
            {
                const Note& n = ed->notes()[i];
                const float x = grid->xOf (n.start);
                if (x < ROLL_KEYW || x > w) continue;
                const bool on = i < (int) ed->sel.size() && ed->sel[i];
                nvgBeginPath (vg); nvgRect (vg, x, h * (1.f - n.vel), 2.5f, h * n.vel);
                nvgFillColor (vg, on ? rollCol (0xc8, 0xff, 0x3a) : rollCol (0x5a, 0xc8, 0xe6)); nvgFill (vg);
            }
        std::shared_ptr<Font> font = APP->window->loadFont (asset::system ("res/fonts/ShareTechMono-Regular.ttf"));
        if (font && font->handle >= 0)
        {
            nvgFontFaceId (vg, font->handle); nvgFontSize (vg, 8.f); nvgTextAlign (vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor (vg, rollCol (0x8d, 0x8d, 0x96)); nvgText (vg, 1.f, h / 2, "VEL", nullptr);
        }
        nvgResetScissor (vg);
        nvgRestore (vg);
    }
};

// One LED-style display in the tool strip: left click = next value, right click = pick from a list.
struct StripCell : OpaqueWidget
{
    std::function<std::string()> text;
    std::function<void()> click;
    std::function<void (Menu*)> list;

    void onButton (const ButtonEvent& e) override
    {
        if (e.action != GLFW_PRESS) return;
        if (e.button == GLFW_MOUSE_BUTTON_LEFT && click) { click(); e.consume (this); }
        else if (e.button == GLFW_MOUSE_BUTTON_RIGHT && list) { list (createMenu()); e.consume (this); }
    }

    void drawLayer (const DrawArgs& args, int layer) override
    {
        if (layer != 1) return;
        NVGcontext* vg = args.vg;
        nvgBeginPath (vg); nvgRoundedRect (vg, 0, 0, box.size.x, box.size.y, 2.f);
        nvgFillColor (vg, rollCol (0x0c, 0x0c, 0x0e)); nvgFill (vg);
        nvgStrokeColor (vg, rollCol (0x8d, 0x8d, 0x96, 120)); nvgStrokeWidth (vg, 0.6f); nvgStroke (vg);
        std::shared_ptr<Font> font = APP->window->loadFont (asset::system ("res/fonts/ShareTechMono-Regular.ttf"));
        if (font && font->handle >= 0 && text)
        {
            nvgFontFaceId (vg, font->handle); nvgFontSize (vg, 11.f); nvgTextAlign (vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor (vg, rollCol (0xc8, 0xff, 0x3a)); nvgText (vg, box.size.x / 2, box.size.y / 2, text().c_str(), nullptr);
        }
    }
};
