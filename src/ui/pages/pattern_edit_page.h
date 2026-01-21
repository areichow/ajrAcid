#pragma once
#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include <array>
#include <vector>

class BankSelectionBarComponent;
class PatternSelectionBarComponent;

class PatternEditPage : public IPage, public IMultiHelpFramesProvider {
 public:
  PatternEditPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;
  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

  int activePatternCursor() const;
  int activePatternStep() const;
  void setPatternCursor(int cursorIndex);
  void focusPatternRow();
  void focusPatternSteps();
  bool patternRowFocused() const;
  void movePatternCursor(int delta);
  void movePatternCursorVertical(int delta);
  int voiceIndex() const { return voice_index_; }

 private:
  enum class Focus { Steps = 0, PatternRow, BankRow };

	// undo/redo state
  struct PatternState {
    int8_t notes[SEQ_STEPS];
    bool accent[SEQ_STEPS];
    bool slide[SEQ_STEPS];
  };

	// undo/redo - capturing snapshots
  void captureCurrentState(PatternState& out) const;
  void applyStateNoTrack(const PatternState& st); // apply without affecting undo/redo
  int activeBankIndex() const;    // current bank from engine
  int activePatternIndex() const; // current pattern from engine

 	// edit scoping
  void beginEdit();
  void endEdit();
  void markChanged();

	// write operations
  void adjustStepNote(int step, int delta);
  void adjustStepOctave(int step, int delta);
  void clearStepNote(int step);
  void toggleAccentStep(int step);
  void toggleSlideStep(int step);

  int clampCursor(int cursorIndex) const;
  int activeBankCursor() const;
  int patternIndexFromKey(char key) const;
  int bankIndexFromKey(char key) const;
  void setBankIndex(int bankIndex);
  void ensureStepFocus();
  void withAudioGuard(const std::function<void()>& fn);

  void setStepNoteAbsolute(int step, int target_note);
  void transposePattern(int semitoneDelta);
  void rotatePattern(int delta); // +1 = forward/right, -1 = backward/left
  void copyFirstHalfToSecondHalf();

  // undo/redo commands
  bool canUndo() const;
  bool canRedo() const;
  void undo();
  void redo();

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  int voice_index_;
  int pattern_edit_cursor_;
  int pattern_row_cursor_;
  int bank_index_;
  int bank_cursor_;
  Focus focus_;
  std::string title_;
  std::shared_ptr<PatternSelectionBarComponent> pattern_bar_;
  std::shared_ptr<BankSelectionBarComponent> bank_bar_;

  int last_note_entered_; // remembers last explicit note value placed/edited

  // undo/redo containers: per-bank, per-pattern for this voice
  using PatternStack = std::vector<PatternState>;
  std::array<std::array<PatternStack, Bank<SynthPattern>::kPatterns>, kBankCount> undo_;
  std::array<std::array<PatternStack, Bank<SynthPattern>::kPatterns>, kBankCount> redo_;

	// edit scope bookkeeping
  int edit_depth_ = 0;
  bool edit_changed_ = false;
  PatternState pre_edit_state_;
};
