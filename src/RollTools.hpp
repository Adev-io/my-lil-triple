// RollTools.hpp: My lil Roll's TOOLS menu: one-click tools, and tool submenus whose sliders preview live. UI thread only.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include "RollUi.hpp"
#include <memory>

typedef std::function<void (std::vector<Note>&, std::vector<char>&, const ToolCtx&)> ToolFn;

// One open tool submenu: the notes as they were when it opened, and the tool to re-run on them.
// Every preview starts again from that snapshot, so slider moves never stack.
struct ToolSession
{
    RollEditor* ed = nullptr;
    std::vector<Note> notes0;
    std::vector<char> sel0;
    int gen0 = 0;
    ToolFn tool;
    bool done = false;   // applied, cancelled, or replaced by a newer session

    bool live() const { return ! done && ed->m && ed->m->docGen == gen0; }   // a preset load or Initialize ends it
    void run()
    {
        if (! live()) return;
        ed->notes() = notes0;
        ed->sel = sel0;
        tool (ed->notes(), ed->sel, ed->toolCtx());
        ed->commit();
    }
    void finish() { done = true; ed->previewing = false; }
    void apply() { if (live()) ed->record (notes0); finish(); }
    void cancel()
    {
        if (live()) { ed->notes() = notes0; ed->sel = sel0; ed->commit(); }
        if (! done) finish();
    }
};

// The newest session. Rack builds a submenu before it deletes the one it replaces, so a new session must put the
// notes back from the old one before it takes its own snapshot.
inline std::weak_ptr<ToolSession>& liveSession() { static std::weak_ptr<ToolSession> s; return s; }
inline void endToolSession (RollEditor* only = nullptr)
{
    if (std::shared_ptr<ToolSession> s = liveSession().lock())
        if (! only || s->ed == only) s->cancel();
}

// Zero-size child of a tool submenu. Rack deletes a submenu's children with it (when the menu closes, or another
// submenu replaces it), so this reverts the preview unless Apply ran.
struct RevertGuard : widget::Widget
{
    std::shared_ptr<ToolSession> s;
    RevertGuard() { box.size = Vec(); }   // Rack's default widget size is infinite, which made the whole submenu infinite
    ~RevertGuard() { s->cancel(); }
};

// One slider: a float or int field of the tool's params. Ints keep the unrounded drag position, so slow drags move.
struct ToolQuantity : Quantity
{
    std::shared_ptr<ToolSession> s;
    std::string label, unit;
    float* f = nullptr;
    int* i = nullptr;
    float lo = 0.f, hi = 1.f, def = 0.f, disp = 1.f;   // shown = value x disp, with `digits` decimals
    int digits = 0;
    std::function<std::string (int)> name;             // ints: shown as a name (snap lists, notes)
    float acc = 0.f;

    float getValue() override
    {
        if (f) return *f;
        if ((int) std::lround (acc) != *i) acc = (float) *i;   // the field changed elsewhere (Reset to defaults)
        return acc;
    }
    void setValue (float v) override
    {
        v = clamp (v, lo, hi);
        if (f)
        {
            if (*f == v) return;
            *f = v;
        }
        else
        {
            acc = v;
            const int r = (int) std::lround (v);
            if (r == *i) return;
            *i = r;
        }
        s->run();
    }
    float getMinValue() override { return lo; }
    float getMaxValue() override { return hi; }
    float getDefaultValue() override { return def; }
    std::string getLabel() override { return label; }
    std::string getUnit() override { return name ? "" : unit; }
    std::string getDisplayValueString() override
    {
        if (i) return name ? name (*i) : string::f ("%d", *i);
        return string::f ("%.*f", digits, *f * disp);
    }
};

// Owns its quantity, like the sliders in Rack's own View menu.
struct ToolSlider : ui::Slider
{
    ToolSlider() { box.size = Vec (240.f, BND_WIDGET_HEIGHT); }
    ~ToolSlider() { delete quantity; }
};

// A menu item that acts and keeps the menu open (toggles and choices inside a tool submenu).
struct StayItem : ui::MenuItem
{
    std::function<void()> action;
    std::function<bool()> checked;
    void step() override
    {
        if (checked) rightText = checked() ? CHECKMARK_STRING : "";
        ui::MenuItem::step();
    }
    void onAction (const ActionEvent& e) override
    {
        if (action) action();
        e.unconsume();   // MenuItem pre-consumes; unconsumed, the menu stays open
    }
};

// A submenu item; clicking it does nothing (the default would close the whole menu and cancel the preview).
struct ToolMenuItem : ui::MenuItem
{
    std::function<void (ui::Menu*)> build;
    ui::Menu* createChildMenu() override
    {
        ui::Menu* m = new ui::Menu;
        build (m);
        return m;
    }
    void onAction (const ActionEvent& e) override { e.unconsume(); }
};

// Remembered for the whole Rack session (one set per process), so each tool reopens with its last values.
struct ToolParams
{
    QuantizeP quant; int quantGrid = 0;   // 0: take the strip's snap on first open
    ChopP chop; int chopSlice = SNAP_DEFAULT;
    ArticulateP art; int artPreset = ART_LEGATO;
    ArpP arp; int arpStep = SNAP_DEFAULT;
    StrumP strum;
    FlamP flam;
    FlipP flip;
    LimitP limit;
    RandomizeP rnd;
    ScaleLevelsP levels;
    LfoP lfo;
};
inline ToolParams& toolParams() { static ToolParams p; return p; }

inline std::string noteName (int p) { return string::f ("%s%d", KEY_NAMES[((p % 12) + 12) % 12], p / 12 - 1); }
inline std::string snapName (int i) { return SNAPS[i].name; }
inline int stripSnapIndex (RollEditor* e) { return e->m->doc.snap > 0 ? e->m->doc.snap : SNAP_DEFAULT; }   // OFF -> 1/16

// Builders for the controls of one tool submenu.
struct ToolMenu
{
    ui::Menu* menu;
    std::shared_ptr<ToolSession> s;

    void slider (std::string label, float& v, float lo, float hi, float def, float disp = 1.f, int digits = 0, std::string unit = "")
    {
        ToolQuantity* q = new ToolQuantity;
        q->s = s; q->label = label; q->unit = unit; q->f = &v;
        q->lo = lo; q->hi = hi; q->def = def; q->disp = disp; q->digits = digits;
        add (q);
    }
    void percent (std::string label, float& v, float lo, float hi, float def) { slider (label, v, lo, hi, def, 100.f, 0, "%"); }
    void islider (std::string label, int& v, int lo, int hi, int def, std::string unit = "", std::function<std::string (int)> name = nullptr)
    {
        ToolQuantity* q = new ToolQuantity;
        q->s = s; q->label = label; q->unit = unit; q->i = &v;
        q->lo = (float) lo; q->hi = (float) hi; q->def = (float) def; q->name = name; q->acc = (float) v;
        add (q);
    }
    void snapSlider (std::string label, int& v, int def) { islider (label, v, 1, SNAP_N - 1, def, "", snapName); }   // the snap list without OFF
    void add (ToolQuantity* q)
    {
        ToolSlider* w = new ToolSlider;
        w->quantity = q;
        menu->addChild (w);
    }
    void item (std::string text, std::function<bool()> checked, std::function<void()> action)
    {
        StayItem* it = createMenuItem<StayItem> (text);
        it->checked = checked;
        it->action = action;
        menu->addChild (it);
    }
    void toggle (std::string text, bool& v)
    {
        std::shared_ptr<ToolSession> ss = s;
        bool* p = &v;
        item (text, [=] { return *p; }, [=] { *p = ! *p; ss->run(); });
    }
    void choice (std::string title, std::vector<std::string> names, int& v)
    {
        menu->addChild (createMenuLabel (title));
        std::shared_ptr<ToolSession> ss = s;
        int* p = &v;
        for (int k = 0; k < (int) names.size(); ++k)
            item ("  " + names[k], [=] { return *p == k; }, [=] { *p = k; ss->run(); });
    }
};

// A tool submenu: opening it snapshots the notes and previews the tool at once with the remembered values.
// Apply = one undo step; closing the menu any other way puts the notes back.
inline ui::MenuItem* toolSubmenu (std::string name, RollEditor* e, ToolFn tool, std::function<void (ToolMenu&)> controls, std::function<void()> reset)
{
    ToolMenuItem* it = createMenuItem<ToolMenuItem> (name, RIGHT_ARROW);
    it->build = [=] (ui::Menu* menu) {
        endToolSession();
        e->syncSel();
        std::shared_ptr<ToolSession> s = std::make_shared<ToolSession>();
        s->ed = e;
        s->notes0 = e->notes();
        s->sel0 = e->sel;
        s->gen0 = e->m->docGen;
        s->tool = tool;
        liveSession() = s;
        e->previewing = true;

        ToolMenu tm { menu, s };
        controls (tm);
        menu->addChild (new MenuSeparator);
        menu->addChild (createMenuItem ("Apply", "", [=] { s->apply(); }, false, true));   // always closes the menu, Ctrl or not
        tm.item ("Reset to defaults", nullptr, [=] { reset(); s->run(); });
        RevertGuard* g = new RevertGuard;
        g->s = s;
        menu->addChild (g);
        s->run();
    };
    return it;
}

// One click: one undo step.
inline void runOnce (RollEditor* e, ToolFn f)
{
    endToolSession();
    e->syncSel();
    const std::vector<Note> before = e->notes();
    f (e->notes(), e->sel, e->toolCtx());
    e->commit();
    e->record (before);
}

inline void appendTools (ui::Menu* menu, RollEditor* e)
{
    ToolParams& P = toolParams();
    const std::string C = RACK_MOD_CTRL_NAME "+", S = RACK_MOD_SHIFT_NAME "+";
    auto once = [=] (std::string text, std::string key, ToolFn f) { menu->addChild (createMenuItem (text, key, [=] { runOnce (e, f); })); };
    once ("Quick quantize start times", S + "Q", quickQuantizeStarts);
    once ("Quick quantize", "", quickQuantize);
    once ("Quick chop", C + "U", quickChop);
    once ("Quick legato", C + "L", quickLegato);
    once ("Glue", C + "G", glue);
    once ("Discard lengths", S + "D", discardLengths);
    once ("Flip horizontal", "", [] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { flip (n, s, FlipP(), c); });
    once ("Flip vertical", "", [] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) {
        FlipP v; v.horizontal = false; v.vertical = true; flip (n, s, v, c); });
    once ("Rotate left", S + C + "Left", [] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { rotate (n, s, c, -1); });
    once ("Rotate right", S + C + "Right", [] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { rotate (n, s, c, 1); });
    menu->addChild (new MenuSeparator);

    ToolParams* p = &P;
    menu->addChild (toolSubmenu ("Quantize", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { QuantizeP q = p->quant; q.grid = SNAPS[p->quantGrid].ticks; quantize (n, s, q, c); },
        [=] (ToolMenu& m) {
            if (p->quantGrid < 1) p->quantGrid = stripSnapIndex (e);
            m.snapSlider ("Grid", p->quantGrid, stripSnapIndex (e));
            m.percent ("Start strength", p->quant.start, 0.f, 1.f, 1.f);
            m.percent ("Sensitivity", p->quant.sens, 0.f, 1.f, 1.f);
            m.choice ("Length", { "Leave length", "Quantize length", "Leave end", "Quantize end" }, p->quant.lenMode);
            m.percent ("Length strength", p->quant.len, 0.f, 1.f, 1.f);
        },
        [=] { p->quant = QuantizeP(); p->quantGrid = stripSnapIndex (e); }));
    menu->addChild (toolSubmenu ("Chop", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { ChopP q = p->chop; q.slice = SNAPS[p->chopSlice].ticks; chop (n, s, q, c); },
        [=] (ToolMenu& m) {
            m.snapSlider ("Slice", p->chopSlice, SNAP_DEFAULT);
            m.toggle ("Absolute (slice on the loop grid)", p->chop.absolute);
            m.percent ("Gate", p->chop.gate, 0.1f, 1.f, 1.f);
        },
        [=] { p->chop = ChopP(); p->chopSlice = SNAP_DEFAULT; }));
    menu->addChild (toolSubmenu ("Articulate", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { articulate (n, s, p->art, c); },
        [=] (ToolMenu& m) {
            m.menu->addChild (createMenuLabel ("Preset"));
            const std::vector<std::string> names = { "Legato", "Portato", "Staccato", "Small gap", "Chop chords" };
            std::shared_ptr<ToolSession> ss = m.s;
            for (int k = 0; k < (int) names.size(); ++k)
                m.item ("  " + names[k], [=] { return p->artPreset == k; }, [=] {
                    const float var = p->art.variation;
                    const int seed = p->art.seed;
                    p->art = articulatePreset (k);
                    p->art.variation = var;
                    p->art.seed = seed;
                    p->artPreset = k;
                    ss->run();
                });
            m.percent ("Length", p->art.length, 0.1f, 1.f, 1.f);
            m.percent ("Variation", p->art.variation, 0.f, 1.f, 0.f);
            m.islider ("Seed", p->art.seed, 1, 99, 1);
            m.toggle ("Use original lengths", p->art.useLengths);
            m.toggle ("Chop chords", p->art.chopChords);
        },
        [=] { p->art = ArticulateP(); p->artPreset = ART_LEGATO; }));
    menu->addChild (toolSubmenu ("Arpeggiate", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { ArpP q = p->arp; q.step = SNAPS[p->arpStep].ticks; arpeggiate (n, s, q, c); },
        [=] (ToolMenu& m) {
            m.choice ("Pattern", { "Up", "Down", "Up-down", "Down-up", "As played", "Random" }, p->arp.pattern);
            m.snapSlider ("Step", p->arpStep, SNAP_DEFAULT);
            m.islider ("Octaves", p->arp.octaves, 1, 4, 1);
            m.choice ("Range order", { "Normal", "Flip", "Alternate" }, p->arp.order);
            m.percent ("Gate", p->arp.gate, 0.1f, 1.f, 0.9f);
            m.islider ("Seed", p->arp.seed, 1, 99, 1);
        },
        [=] { p->arp = ArpP(); p->arpStep = SNAP_DEFAULT; }));
    menu->addChild (toolSubmenu ("Strum", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { strum (n, s, p->strum, c); },
        [=] (ToolMenu& m) {
            m.slider ("Time (+ low to high)", p->strum.time, -1.f, 1.f, 0.25f, 1.f, 2, " beat");
            m.slider ("Tension", p->strum.tension, -1.f, 1.f, 0.f, 1.f, 2);
            m.percent ("Velocity fall", p->strum.velFall, 0.f, 1.f, 0.f);
            m.slider ("Velocity tension", p->strum.velTension, -1.f, 1.f, 0.f, 1.f, 2);
            m.toggle ("Preserve end", p->strum.preserveEnd);
            m.toggle ("Alternate direction", p->strum.alternate);
        },
        [=] { p->strum = StrumP(); }));
    menu->addChild (toolSubmenu ("Flam", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { flam (n, s, p->flam, c); },
        [=] (ToolMenu& m) {
            m.islider ("Time", p->flam.time, 1, 48, 12, " ticks");
            m.toggle ("Before the note", p->flam.before);
            m.percent ("Grace velocity", p->flam.vel, 0.f, 1.f, 0.6f);
        },
        [=] { p->flam = FlamP(); }));
    menu->addChild (toolSubmenu ("Flip", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { flip (n, s, p->flip, c); },
        [=] (ToolMenu& m) {
            m.toggle ("Horizontal", p->flip.horizontal);
            m.toggle ("Vertical", p->flip.vertical);
            m.toggle ("Preserve start times", p->flip.preserveStarts);
        },
        [=] { p->flip = FlipP(); }));
    menu->addChild (toolSubmenu ("Limit", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { limit (n, s, p->limit, c); },
        [=] (ToolMenu& m) {
            m.islider ("Low", p->limit.low, 0, 127, 48, "", noteName);
            m.islider ("High", p->limit.high, 0, 127, 72, "", noteName);
            m.toggle ("Wrap", p->limit.wrap);
            m.choice ("Scale fix (strip key + mode)", { "Off", "Above", "Below", "Alternate" }, p->limit.scaleFix);
        },
        [=] { p->limit = LimitP(); }));
    menu->addChild (toolSubmenu ("Randomize", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { randomize (n, s, p->rnd, c); },
        [=] (ToolMenu& m) {
            m.toggle ("Levels", p->rnd.levels);
            m.percent ("Velocity amount", p->rnd.velAmount, 0.f, 1.f, 0.2f);
            m.toggle ("Bipolar", p->rnd.bipolar);
            m.toggle ("Pattern", p->rnd.pattern);
            m.percent ("Population", p->rnd.population, 0.f, 1.f, 0.5f);
            m.islider ("Stack", p->rnd.stack, 1, 4, 1, " notes");
            m.islider ("Octave", p->rnd.octave, 1, 7, 4, "", [] (int o) { return string::f ("C%d", o); });
            m.islider ("Range", p->rnd.range, 1, 3, 1, " oct");
            m.percent ("Length variation", p->rnd.lenVar, -1.f, 1.f, 0.f);
            m.toggle ("Merge same notes", p->rnd.merge);
            m.islider ("Seed", p->rnd.seed, 1, 99, 1);
        },
        [=] { p->rnd = RandomizeP(); }));
    menu->addChild (toolSubmenu ("Scale levels", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { scaleLevels (n, s, p->levels, c); },
        [=] (ToolMenu& m) {
            m.slider ("Center", p->levels.center, -100.f, 100.f, 0.f);
            m.percent ("Multiply", p->levels.multiply, 0.f, 2.f, 1.f);
            m.slider ("Tension", p->levels.tension, -100.f, 100.f, 0.f);
            m.percent ("Offset", p->levels.offset, -1.f, 1.f, 0.f);
        },
        [=] { p->levels = ScaleLevelsP(); }));
    menu->addChild (toolSubmenu ("Velocity LFO", e,
        [=] (std::vector<Note>& n, std::vector<char>& s, const ToolCtx& c) { velocityLfo (n, s, p->lfo, c); },
        [=] (ToolMenu& m) {
            m.choice ("Shape", { "Sine", "Triangle", "Square" }, p->lfo.shape);
            m.slider ("Cycles", p->lfo.cycles, 0.25f, 16.f, 1.f, 1.f, 2);
            m.slider ("Phase", p->lfo.phase, 0.f, 360.f, 0.f, 1.f, 0, " deg");
            m.percent ("Depth", p->lfo.depth, 0.f, 1.f, 0.5f);
            m.percent ("Center", p->lfo.center, 0.f, 1.f, 0.5f);
        },
        [=] { p->lfo = LfoP(); }));
}
