#include "pattern_edit_page.h"
#include <cctype>
#include <utility>
#include "../help_dialog_frames.h"
#include "../components/bank_selection_bar.h"
#include "../components/pattern_selection_bar.h"

namespace {
struct PatternClipboard {
  bool has_pattern = false;
  SynthPattern pattern{};
};
PatternClipboard g_pattern_clipboard;
} // namespace

// ==== ctor ===================================================================
PatternEditPage::PatternEditPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index)
 : gfx_(gfx),
   mini_acid_(mini_acid),
   audio_guard_(audio_guard),
   voice_index_(voice_index),
   pattern_edit_cursor_(0),
   pattern_row_cursor_(0),
   bank_index_(0),
   bank_cursor_(0),
   focus_(Focus::Steps),
   last_note_entered_(MiniAcid::kMin303Note) // default to base C1
{
  int idx = mini_acid_.current303PatternIndex(voice_index_);
  if (idx < 0 || idx >= Bank<SynthPattern>::kPatterns) idx = 0;
  pattern_row_cursor_ = idx;
  bank_index_ = mini_acid_.current303BankIndex(voice_index_);
  bank_cursor_ = bank_index_;
  title_ = voice_index_ == 0 ? "303A PATTERNS" : "303B PATTERNS";

  pattern_bar_ = std::make_shared<PatternSelectionBarComponent>("PATTERNS");
  bank_bar_ = std::make_shared<BankSelectionBarComponent>("BANK", "ABCD");

  PatternSelectionBarComponent::Callbacks pattern_callbacks;
  pattern_callbacks.onSelect = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    focusPatternRow();
    setPatternCursor(index);
    withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, index); });
  };
  pattern_bar_->setCallbacks(std::move(pattern_callbacks));

  BankSelectionBarComponent::Callbacks bank_callbacks;
  bank_callbacks.onSelect = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    focus_ = Focus::BankRow;
    bank_cursor_ = index;
    setBankIndex(index);
  };
  bank_bar_->setCallbacks(std::move(bank_callbacks));
}

// ==== small helpers ==========================================================
int PatternEditPage::clampCursor(int cursorIndex) const {
  int cursor = cursorIndex;
  if (cursor < 0) cursor = 0;
  if (cursor >= Bank<SynthPattern>::kPatterns)
    cursor = Bank<SynthPattern>::kPatterns - 1;
  return cursor;
}
int PatternEditPage::activeBankCursor() const {
  int cursor = bank_cursor_;
  if (cursor < 0) cursor = 0;
  if (cursor >= kBankCount) cursor = kBankCount - 1;
  return cursor;
}
int PatternEditPage::patternIndexFromKey(char key) const {
  switch (std::tolower(static_cast<unsigned char>(key))) {
    case 'q': return 0;
    case 'w': return 1;
    case 'e': return 2;
    case 'r': return 3;
    case 't': return 4;
    case 'y': return 5;
    case 'u': return 6;
    case 'i': return 7;
    default: return -1;
  }
}
int PatternEditPage::bankIndexFromKey(char key) const {
  switch (key) {
    case '1': return 0;
    case '2': return 1;
    case '3': return 2;
    case '4': return 3;
    default: return -1;
  }
}
void PatternEditPage::setBankIndex(int bankIndex) {
  if (bankIndex < 0) bankIndex = 0;
  if (bankIndex >= kBankCount) bankIndex = kBankCount - 1;
  if (bank_index_ == bankIndex) return;
  bank_index_ = bankIndex;
  withAudioGuard([&]() { mini_acid_.set303BankIndex(voice_index_, bank_index_); });
}
void PatternEditPage::ensureStepFocus() {
  if (patternRowFocused() || focus_ == Focus::BankRow) focus_ = Focus::Steps;
}
void PatternEditPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) { audio_guard_(fn); return; }
  fn();
}
int PatternEditPage::activePatternCursor() const { return clampCursor(pattern_row_cursor_); }
int PatternEditPage::activePatternStep() const {
  int idx = pattern_edit_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= SEQ_STEPS) idx = SEQ_STEPS - 1;
  return idx;
}
void PatternEditPage::setPatternCursor(int cursorIndex) { pattern_row_cursor_ = clampCursor(cursorIndex); }
void PatternEditPage::focusPatternRow() {
  if (mini_acid_.songModeEnabled()) return;
  setPatternCursor(pattern_row_cursor_);
  focus_ = Focus::PatternRow;
}
void PatternEditPage::focusPatternSteps() {
  int row = pattern_edit_cursor_ / 8;
  if (row < 0 || row > 1) row = 0;
  pattern_edit_cursor_ = row * 8 + activePatternCursor();
  focus_ = Focus::Steps;
}
bool PatternEditPage::patternRowFocused() const {
  if (mini_acid_.songModeEnabled()) return false;
  return focus_ == Focus::PatternRow;
}
void PatternEditPage::movePatternCursor(int delta) {
  if (mini_acid_.songModeEnabled() && focus_ == Focus::PatternRow) { focus_ = Focus::Steps; }
  if (focus_ == Focus::BankRow) {
    int cursor = activeBankCursor();
    cursor = (cursor + delta) % kBankCount;
    if (cursor < 0) cursor += kBankCount;
    bank_cursor_ = cursor;
    return;
  }
  if (focus_ == Focus::PatternRow) {
    int cursor = activePatternCursor();
    cursor = (cursor + delta) % Bank<SynthPattern>::kPatterns;
    if (cursor < 0) cursor += Bank<SynthPattern>::kPatterns;
    pattern_row_cursor_ = cursor;
    return;
  }
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  col = (col + delta) % 8;
  if (col < 0) col += 8;
  pattern_edit_cursor_ = row * 8 + col;
}
void PatternEditPage::movePatternCursorVertical(int delta) {
  if (delta == 0) return;
  if (mini_acid_.songModeEnabled() && focus_ == Focus::PatternRow) { focus_ = Focus::Steps; }
  if (focus_ == Focus::BankRow) {
    if (delta > 0) { focus_ = mini_acid_.songModeEnabled() ? Focus::Steps : Focus::PatternRow; }
    return;
  }
  if (focus_ == Focus::PatternRow) {
    if (delta < 0 && !mini_acid_.songModeEnabled()) {
      bank_cursor_ = bank_index_;
      focus_ = Focus::BankRow;
      return;
    }
    int col = activePatternCursor();
    int targetRow = delta > 0 ? 0 : 1;
    pattern_edit_cursor_ = targetRow * 8 + col;
    focus_ = Focus::Steps;
    return;
  }
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  int newRow = row + delta;
  if (newRow < 0) {
    if (mini_acid_.songModeEnabled()) newRow = 0;
    else { focus_ = Focus::PatternRow; setPatternCursor(col); return; }
  }
  if (newRow > 1) {
    if (mini_acid_.songModeEnabled()) newRow = 1;
    else { focus_ = Focus::PatternRow; setPatternCursor(col); return; }
  }
  pattern_edit_cursor_ = newRow * 8 + col;
}
const std::string & PatternEditPage::getTitle() const { return title_; }

// ==== Undo/Redo internals =====================================================
int PatternEditPage::activeBankIndex() const { return mini_acid_.current303BankIndex(voice_index_); }
int PatternEditPage::activePatternIndex() const {
  int idx = mini_acid_.current303PatternIndex(voice_index_);
  if (idx < 0 || idx >= Bank<SynthPattern>::kPatterns) idx = 0;
  return idx;
}
void PatternEditPage::captureCurrentState(PatternState& out) const {
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide  = mini_acid_.pattern303SlideSteps(voice_index_);
  for (int i = 0; i < SEQ_STEPS; ++i) {
    out.notes[i]  = notes[i];
    out.accent[i] = accent[i];
    out.slide[i]  = slide[i];
  }
}
void PatternEditPage::applyStateNoTrack(const PatternState& st) {
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide  = mini_acid_.pattern303SlideSteps(voice_index_);

  withAudioGuard([&]() {
    for (int i = 0; i < SEQ_STEPS; ++i) {
      int target_note = st.notes[i];
      int current_note = notes[i];
      if (target_note < 0) {
        if (current_note >= 0) mini_acid_.clear303StepNote(voice_index_, i);
      } else if (current_note < 0) {
        int delta = target_note - MiniAcid::kMin303Note;
        if (delta == 0) {
          mini_acid_.adjust303StepNote(voice_index_, i, 1);
          mini_acid_.adjust303StepNote(voice_index_, i, -1);
        } else {
          mini_acid_.adjust303StepNote(voice_index_, i, delta);
        }
      } else {
        int delta = target_note - current_note;
        if (delta != 0) mini_acid_.adjust303StepNote(voice_index_, i, delta);
      }
      if (accent[i] != st.accent[i]) mini_acid_.toggle303AccentStep(voice_index_, i);
      if (slide[i]  != st.slide[i])  mini_acid_.toggle303SlideStep(voice_index_, i);
    }
  });
}

void PatternEditPage::beginEdit() {
  if (++edit_depth_ == 1) { captureCurrentState(pre_edit_state_); edit_changed_ = false; }
}
void PatternEditPage::endEdit() {
  if (edit_depth_ == 0) return;
  if (--edit_depth_ == 0) {
    if (edit_changed_) {
      int b = activeBankIndex();
      int p = activePatternIndex();
      undo_[b][p].push_back(pre_edit_state_);
      redo_[b][p].clear();
    }
  }
}
void PatternEditPage::markChanged() { if (edit_depth_ > 0) edit_changed_ = true; }

// ---- Write wrappers ---------------------------------------------------------
void PatternEditPage::adjustStepNote(int step, int delta) {
  if (delta == 0) return;
  markChanged();
  withAudioGuard([&]() { mini_acid_.adjust303StepNote(voice_index_, step, delta); });
}
void PatternEditPage::adjustStepOctave(int step, int delta) {
  if (delta == 0) return;
  markChanged();
  withAudioGuard([&]() { mini_acid_.adjust303StepOctave(voice_index_, step, delta); });
}
void PatternEditPage::clearStepNote(int step) {
  markChanged();
  withAudioGuard([&]() { mini_acid_.clear303StepNote(voice_index_, step); });
}
void PatternEditPage::toggleAccentStep(int step) {
  markChanged();
  withAudioGuard([&]() { mini_acid_.toggle303AccentStep(voice_index_, step); });
}
void PatternEditPage::toggleSlideStep(int step) {
  markChanged();
  withAudioGuard([&]() { mini_acid_.toggle303SlideStep(voice_index_, step); });
}

// ==== Editing ops ============================================================
void PatternEditPage::setStepNoteAbsolute(int step, int target_note) {
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  int current_note = notes[step];

  beginEdit();
  if (target_note < 0) {
    if (current_note >= 0) { clearStepNote(step); }
    endEdit();
    return;
  }

  if (current_note < 0) {
    int delta = target_note - MiniAcid::kMin303Note;
    if (delta == 0) { adjustStepNote(step, 1); adjustStepNote(step, -1); }
    else { adjustStepNote(step, delta); }
  } else {
    int delta = target_note - current_note;
    if (delta != 0) adjustStepNote(step, delta);
  }
  endEdit();
}

void PatternEditPage::transposePattern(int semitoneDelta) {
  if (semitoneDelta == 0) return;
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  beginEdit();
  for (int i = 0; i < SEQ_STEPS; ++i) if (notes[i] >= 0) adjustStepNote(i, semitoneDelta);
  endEdit();
}

void PatternEditPage::rotatePattern(int delta) {
  if (delta == 0) return;
  int current_notes[SEQ_STEPS];
  bool current_accent[SEQ_STEPS];
  bool current_slide[SEQ_STEPS];
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide  = mini_acid_.pattern303SlideSteps(voice_index_);
  for (int i = 0; i < SEQ_STEPS; ++i) { current_notes[i] = notes[i]; current_accent[i] = accent[i]; current_slide[i] = slide[i]; }
  int target_notes[SEQ_STEPS];
  bool target_accent[SEQ_STEPS];
  bool target_slide[SEQ_STEPS];
  const int shift = (delta > 0) ? 1 : -1; // +1 = forward/right, -1 = backward/left
  for (int i = 0; i < SEQ_STEPS; ++i) {
    int src = (i - shift + SEQ_STEPS) % SEQ_STEPS;
    target_notes[i]  = current_notes[src];
    target_accent[i] = current_accent[src];
    target_slide[i]  = current_slide[src];
  }
  beginEdit();
  for (int i = 0; i < SEQ_STEPS; ++i) {
    int target_note  = target_notes[i];
    int current_note = current_notes[i];
    if (target_note < 0) { if (current_note >= 0) clearStepNote(i); }
    else if (current_note < 0) {
      int deltaN = target_note - MiniAcid::kMin303Note;
      if (deltaN == 0) { adjustStepNote(i, 1); adjustStepNote(i, -1); }
      else { adjustStepNote(i, deltaN); }
    } else {
      int deltaN = target_note - current_note;
      if (deltaN != 0) adjustStepNote(i, deltaN);
    }
    if (current_accent[i] != target_accent[i]) toggleAccentStep(i);
    if (current_slide[i]  != target_slide[i])  toggleSlideStep(i);
  }
  endEdit();
}

void PatternEditPage::copyFirstHalfToSecondHalf() {
  int current_notes[SEQ_STEPS];
  bool current_accent[SEQ_STEPS];
  bool current_slide[SEQ_STEPS];
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide  = mini_acid_.pattern303SlideSteps(voice_index_);
  for (int i = 0; i < SEQ_STEPS; ++i) { current_notes[i] = notes[i]; current_accent[i] = accent[i]; current_slide[i] = slide[i]; }
  beginEdit();
  for (int i = 8; i < SEQ_STEPS; ++i) {
    int src = i - 8;
    int target_note  = current_notes[src];
    int current_note = current_notes[i];
    if (target_note < 0) { if (current_note >= 0) clearStepNote(i); }
    else if (current_note < 0) {
      int deltaN = target_note - MiniAcid::kMin303Note;
      if (deltaN == 0) { adjustStepNote(i, 1); adjustStepNote(i, -1); }
      else { adjustStepNote(i, deltaN); }
    } else {
      int deltaN = target_note - current_note;
      if (deltaN != 0) adjustStepNote(i, deltaN);
    }
    if (current_accent[i] != current_accent[src]) toggleAccentStep(i);
    if (current_slide[i]  != current_slide[src])  toggleSlideStep(i);
  }
  endEdit();
}

// ==== Undo/Redo public ops ====================================================
bool PatternEditPage::canUndo() const { int b = activeBankIndex(); int p = activePatternIndex(); return !undo_[b][p].empty(); }
bool PatternEditPage::canRedo() const { int b = activeBankIndex(); int p = activePatternIndex(); return !redo_[b][p].empty(); }
void PatternEditPage::undo() {
  int b = activeBankIndex(); int p = activePatternIndex();
  if (undo_[b][p].empty()) return;
  PatternState cur{}; captureCurrentState(cur);
  PatternState prev = undo_[b][p].back(); undo_[b][p].pop_back();
  redo_[b][p].push_back(cur);
  applyStateNoTrack(prev);
}
void PatternEditPage::redo() {
  int b = activeBankIndex(); int p = activePatternIndex();
  if (redo_[b][p].empty()) return;
  PatternState cur{}; captureCurrentState(cur);
  PatternState next = redo_[b][p].back(); redo_[b][p].pop_back();
  undo_[b][p].push_back(cur);
  applyStateNoTrack(next);
}

// ==== Event handling ==========================================================
bool PatternEditPage::handleEvent(UIEvent& ui_event) {
  if (pattern_bar_ && pattern_bar_->handleEvent(ui_event)) return true;
  if (bank_bar_ && bank_bar_->handleEvent(ui_event)) return true;

  if (ui_event.event_type == MINIACID_APPLICATION_EVENT) {
    switch (ui_event.app_event_type) {
      case MINIACID_APP_EVENT_COPY: {
        const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
        const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
        const bool* slide  = mini_acid_.pattern303SlideSteps(voice_index_);
        for (int i = 0; i < SEQ_STEPS; ++i) {
          g_pattern_clipboard.pattern.steps[i].note   = notes[i];
          g_pattern_clipboard.pattern.steps[i].accent = accent[i];
          g_pattern_clipboard.pattern.steps[i].slide  = slide[i];
        }
        g_pattern_clipboard.has_pattern = true;
        return true;
      }
      case MINIACID_APP_EVENT_PASTE: {
        if (!g_pattern_clipboard.has_pattern) return false;
        int current_notes[SEQ_STEPS];
        bool current_accent[SEQ_STEPS];
        bool current_slide[SEQ_STEPS];
        const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
        const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
        const bool* slide  = mini_acid_.pattern303SlideSteps(voice_index_);
        for (int i = 0; i < SEQ_STEPS; ++i) { current_notes[i] = notes[i]; current_accent[i] = accent[i]; current_slide[i] = slide[i]; }
        const SynthPattern& src = g_pattern_clipboard.pattern;

        beginEdit();
        for (int i = 0; i < SEQ_STEPS; ++i) {
          int target_note  = src.steps[i].note;
          int current_note = current_notes[i];
          if (target_note < 0) { if (current_note >= 0) clearStepNote(i); }
          else if (current_note < 0) {
            int delta = target_note - MiniAcid::kMin303Note;
            if (delta == 0) { adjustStepNote(i, 1); adjustStepNote(i, -1); }
            else { adjustStepNote(i, delta); }
          } else {
            int delta = target_note - current_note;
            if (delta != 0) adjustStepNote(i, delta);
          }
          if (current_accent[i] != src.steps[i].accent) toggleAccentStep(i);
          if (current_slide[i]  != src.steps[i].slide)  toggleSlideStep(i);
        }
        endEdit();
        return true;
      }
      default:
        return false;
    }
  }

  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;

  bool handled = false;
  switch (ui_event.scancode) {
    case MINIACID_LEFT:  movePatternCursor(-1); handled = true; break;
    case MINIACID_RIGHT: movePatternCursor(1);  handled = true; break;
    case MINIACID_UP:    movePatternCursorVertical(-1); handled = true; break;
    case MINIACID_DOWN:  movePatternCursorVertical(1);  handled = true; break;
    default: break;
  }
  if (handled) return true;

  char key = ui_event.key;
  if (!key) return false;

  if (key == '\n' || key == '\r') {
    if (focus_ == Focus::BankRow) {
      if (mini_acid_.songModeEnabled()) return true;
      setBankIndex(activeBankCursor());
      return true;
    }
    if (patternRowFocused()) {
      if (mini_acid_.songModeEnabled()) return true;
      int cursor = activePatternCursor();
      setPatternCursor(cursor);
      withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, cursor); });
      return true;
    }
  }

  int patternIdx = patternIndexFromKey(key);
  bool patternKeyReserved = false;
  if (patternIdx >= 0) {
    char lowerKeySel = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    patternKeyReserved = (lowerKeySel == 'q' || lowerKeySel == 'w');
    if (!patternKeyReserved || patternRowFocused()) {
      if (mini_acid_.songModeEnabled()) return true;
      focusPatternRow();
      setPatternCursor(patternIdx);
      withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, patternIdx); });
      return true;
    }
  }

  auto ensureStepFocusAndCursor = [&]() {
    if (patternRowFocused()) { focusPatternSteps(); } else { ensureStepFocus(); }
  };
  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));

  switch (lowerKey) {
    // --- New hotkeys ---
    case 'j': { undo(); return true; }
    case 'm': { redo(); return true; }

    // --- Existing edits ---
    case 'q': { // slide toggle
      ensureStepFocusAndCursor(); int step = activePatternStep(); beginEdit(); toggleSlideStep(step); endEdit(); return true; }
    case 'w': { // accent toggle
      ensureStepFocusAndCursor(); int step = activePatternStep(); beginEdit(); toggleAccentStep(step); endEdit(); return true; }
    case 'a': { // semitone up OR place last on empty
      ensureStepFocusAndCursor(); int step = activePatternStep();
      const int8_t* notes = mini_acid_.pattern303Steps(voice_index_); int current = notes[step];
      beginEdit(); if (current < 0) { setStepNoteAbsolute(step, last_note_entered_); } else { adjustStepNote(step, 1); } endEdit();
      current = mini_acid_.pattern303Steps(voice_index_)[step]; if (current >= 0) last_note_entered_ = current; return true; }
    case 'z': { // semitone down
      ensureStepFocusAndCursor(); int step = activePatternStep(); beginEdit(); adjustStepNote(step, -1); endEdit();
      int current = mini_acid_.pattern303Steps(voice_index_)[step]; if (current >= 0) last_note_entered_ = current; return true; }
    case 's': { // octave up OR place last_note +12 on empty
      ensureStepFocusAndCursor(); int step = activePatternStep(); const int8_t* notes = mini_acid_.pattern303Steps(voice_index_); int current = notes[step];
      beginEdit(); if (current < 0) { setStepNoteAbsolute(step, last_note_entered_ + 12); } else { adjustStepOctave(step, 1); } endEdit();
      current = mini_acid_.pattern303Steps(voice_index_)[step]; if (current >= 0) last_note_entered_ = current; return true; }
    case 'x': { // octave down OR place last_note -12 on empty
      ensureStepFocusAndCursor(); int step = activePatternStep(); const int8_t* notes = mini_acid_.pattern303Steps(voice_index_); int current = notes[step];
      beginEdit(); if (current < 0) { setStepNoteAbsolute(step, last_note_entered_ - 12); } else { adjustStepOctave(step, -1); } endEdit();
      current = mini_acid_.pattern303Steps(voice_index_)[step]; if (current >= 0) last_note_entered_ = current; return true; }
    case 'g': { transposePattern(+1); return true; }
    case 'b': { transposePattern(-1); return true; }
    case 'h': { rotatePattern(+1); return true; }
    case 'n': { rotatePattern(-1); return true; }
    case '\'': { copyFirstHalfToSecondHalf(); return true; }
    default: break;
  }

  if (key == '\b') { ensureStepFocusAndCursor(); int step = activePatternStep(); beginEdit(); clearStepNote(step); endEdit(); return true; }
  return false;
}

// ==== Help dialog API ========================================================
std::unique_ptr<MultiPageHelpDialog> PatternEditPage::getHelpDialog() { return std::make_unique<MultiPageHelpDialog>(*this); }
int PatternEditPage::getHelpFrameCount() const { return 1; }
void PatternEditPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) { case 0: drawHelpPage303PatternEdit(gfx, bounds.x, bounds.y, bounds.w, bounds.h); break; default: break; }
}

// ==== draw() =================================================================
void PatternEditPage::draw(IGfx& gfx) {
  bank_index_ = mini_acid_.current303BankIndex(voice_index_);
  const Rect& bounds = getBoundaries();
  int x = bounds.x; int y = bounds.y; int w = bounds.w; int h = bounds.h;
  int body_y = y + 2; int body_h = h - 2; if (body_h <= 0) return;

  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide  = mini_acid_.pattern303SlideSteps(voice_index_);

  int stepCursor = pattern_edit_cursor_;
  int playing = mini_acid_.currentStep();
  int selectedPattern = mini_acid_.display303PatternIndex(voice_index_);
  bool songMode = mini_acid_.songModeEnabled();
  bool patternFocus = !songMode && patternRowFocused();
  bool bankFocus = !songMode && focus_ == Focus::BankRow;
  bool stepFocus = !patternFocus && !bankFocus;

  int patternCursor = songMode && selectedPattern >= 0 ? selectedPattern : activePatternCursor();
  int bankCursor = activeBankCursor();

  PatternSelectionBarComponent::State pattern_state;
  pattern_state.pattern_count = Bank<SynthPattern>::kPatterns;
  pattern_state.selected_index = selectedPattern;
  pattern_state.cursor_index = patternCursor;
  pattern_state.show_cursor = patternFocus;
  pattern_state.song_mode = songMode;
  pattern_bar_->setState(pattern_state);
  pattern_bar_->setBoundaries(Rect{x, body_y, w, 0});
  int pattern_bar_h = pattern_bar_->barHeight(gfx);
  pattern_bar_->setBoundaries(Rect{x, body_y, w, pattern_bar_h});
  pattern_bar_->draw(gfx);

  BankSelectionBarComponent::State bank_state;
  bank_state.bank_count = kBankCount;
  bank_state.selected_index = bank_index_;
  bank_state.cursor_index = bankCursor;
  bank_state.show_cursor = bankFocus;
  bank_state.song_mode = songMode;
  bank_bar_->setState(bank_state);
  bank_bar_->setBoundaries(Rect{x, body_y - 1, w, 0});
  int bank_bar_h = bank_bar_->barHeight(gfx);
  bank_bar_->setBoundaries(Rect{x, body_y - 1, w, bank_bar_h});
  bank_bar_->draw(gfx);

  int spacing = 4;
  int grid_top = body_y + pattern_bar_h + 6;
  int cell_size = (w - spacing * 7 - 2) / 8; if (cell_size < 12) cell_size = 12;
  int indicator_h = 5; int indicator_gap = 1; int row_height = indicator_h + indicator_gap + cell_size + 4;

  for (int i = 0; i < SEQ_STEPS; ++i) {
    int row = i / 8; int col = i % 8;
    int cell_x = x + col * (cell_size + spacing);
    int cell_y = grid_top + row * row_height;
    int indicator_w = (cell_size - 2) / 2; if (indicator_w < 4) indicator_w = 4;
    int slide_x = cell_x + cell_size - indicator_w; int indicator_y = cell_y;

    gfx.fillRect(cell_x, indicator_y, indicator_w, indicator_h, slide[i] ? COLOR_SLIDE : COLOR_GRAY_DARKER);
    gfx.drawRect(cell_x, indicator_y, indicator_w, indicator_h, COLOR_WHITE);

    gfx.fillRect(slide_x, indicator_y, indicator_w, indicator_h, accent[i] ? COLOR_ACCENT : COLOR_GRAY_DARKER);
    gfx.drawRect(slide_x, indicator_y, indicator_w, indicator_h, COLOR_WHITE);

    int note_box_y = indicator_y + indicator_h + indicator_gap;
    IGfxColor fill = notes[i] >= 0 ? COLOR_303_NOTE : COLOR_GRAY;
    gfx.fillRect(cell_x, note_box_y, cell_size, cell_size, fill);
    gfx.drawRect(cell_x, note_box_y, cell_size, cell_size, COLOR_WHITE);

    if (playing == i) { gfx.drawRect(cell_x - 1, note_box_y - 1, cell_size + 2, cell_size + 2, COLOR_STEP_HILIGHT); }
    if (stepFocus && stepCursor == i) { gfx.drawRect(cell_x - 2, note_box_y - 2, cell_size + 4, cell_size + 4, COLOR_STEP_SELECTED); }

    char note_label[8];
    formatNoteName(notes[i], note_label, sizeof(note_label));
    int tw = textWidth(gfx, note_label);
    int tx = cell_x + (cell_size - tw) / 2;
    int ty = note_box_y + cell_size / 2 - gfx.fontHeight() / 2;
    gfx.drawText(tx, ty, note_label);
  }
}
