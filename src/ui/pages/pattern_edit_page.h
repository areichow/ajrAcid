#pragma once
#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
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

  int clampCursor(int cursorIndex) const;
  int activeBankCursor() const;
  int patternIndexFromKey(char key) const;
  int bankIndexFromKey(char key) const;
  void setBankIndex(int bankIndex);
  void ensureStepFocus();
  void withAudioGuard(const std::function<void()>& fn);

  // === New helpers ===
  void setStepNoteAbsolute(int step, int target_note);
  void transposePattern(int semitoneDelta);
  void rotatePattern(int delta); // +1 = forward/right, -1 = backward/left
  void copyFirstHalfToSecondHalf();

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

  // === New state ===
  int last_note_entered_; // remembers last explicit note value placed/edited
};
