//
// Copyleft 2011 RIME Developers
// License: GPLv3
//
// 2011-10-27 GONG Chen <chen.sst@gmail.com>
//
#include <rime/candidate.h>
#include <rime/common.h>
#include <rime/composition.h>
#include <rime/config.h>
#include <rime/context.h>
#include <rime/engine.h>
#include <rime/key_event.h>
#include <rime/key_table.h>
#include <rime/menu.h>
#include <rime/schema.h>
#include <rime/gear/speller.h>

static const char kRimeAlphabet[] = "zyxwvutsrqponmlkjihgfedcba";

namespace rime {

static bool is_auto_selectable(const shared_ptr<Candidate>& cand,
                               const std::string& input,
                               const std::string& delimiters) {
  return cand->end() == input.length() &&  // reaches end of input
      cand->type() == "table" &&           // is table entry
      input.find_first_of(delimiters, cand->start()) == std::string::npos;
      // no delimiters
}

Speller::Speller(Engine *engine) : Processor(engine),
                                   alphabet_(kRimeAlphabet),
                                   max_code_length_(0),
                                   auto_select_(false),
                                   auto_select_unique_candidate_(false) {
  Config *config = engine->schema()->config();
  if (config) {
    config->GetString("speller/alphabet", &alphabet_);
    config->GetString("speller/delimiter", &delimiter_);
    config->GetString("speller/initials", &initials_);
    config->GetInt("speller/max_code_length", &max_code_length_);
    config->GetBool("speller/auto_select", &auto_select_);
    if (!config->GetBool("speller/auto_select_unique_candidate",
                         &auto_select_unique_candidate_)) {
      auto_select_unique_candidate_ = auto_select_;
    }
  }
  if (initials_.empty()) initials_ = alphabet_;
}

Processor::Result Speller::ProcessKeyEvent(
    const KeyEvent &key_event) {
  if (key_event.release() || key_event.ctrl() || key_event.alt() ||
      key_event.keycode() == XK_space)
    return kNoop;
  int ch = key_event.keycode();
  if (ch <= 0x20 || ch >= 0x7f)  // not a valid key for spelling
    return kNoop;
  Context *ctx = engine_->context();
  if (ctx->IsComposing()) {
    bool is_letter = alphabet_.find(ch) != std::string::npos;
    bool is_delimiter = delimiter_.find(ch) != std::string::npos;
    if (!is_letter && !is_delimiter)
      return kNoop;
    if (is_letter &&             // a letter may cause auto-select
        max_code_length_ > 0 &&  // at a fixed code length
        ctx->HasMenu()) {
      const Segment& seg(ctx->composition()->back());
      const shared_ptr<Candidate> cand = seg.GetSelectedCandidate();
      if (cand) {
        int code_length = static_cast<int>(cand->end() - cand->start());
        if (code_length == max_code_length_ &&       // exceeds max code length
            is_auto_selectable(cand, ctx->input(), delimiter_)) {
          ctx->ConfirmCurrentSelection();
        }
      }
    }
  }
  else {
    if (initials_.find(ch) == std::string::npos)
      return kNoop;
  }
  Segment previous_segment;
  if (auto_select_ && ctx->HasMenu()) {
    previous_segment = ctx->composition()->back();
  }
  DLOG(INFO) << "add to input: '" << (char)ch << "', " << key_event.repr();
  ctx->PushInput(key_event.keycode());
  ctx->ConfirmPreviousSelection();  // so that next BackSpace won't revert
                                    // previous selection
  if (auto_select_unique_candidate_ && ctx->HasMenu()) {
    const Segment& seg(ctx->composition()->back());
    bool unique_candidate = seg.menu->Prepare(2) == 1;
    if (unique_candidate &&
        is_auto_selectable(seg.GetSelectedCandidate(),
                           ctx->input(), delimiter_)) {
      DLOG(INFO) << "auto-select unique candidate.";
      ctx->ConfirmCurrentSelection();
      return kAccepted;
    }
  }
  if (auto_select_ && !ctx->HasMenu() && previous_segment.menu) {
    if (is_auto_selectable(previous_segment.GetSelectedCandidate(),
                           ctx->input().substr(0, previous_segment.end),
                           delimiter_)) {
      DLOG(INFO) << "auto-select previous word";
      ctx->composition()->pop_back();
      ctx->composition()->push_back(previous_segment);
      ctx->ConfirmCurrentSelection();
      return kAccepted;
    }
  }
  return kAccepted;
}

}  // namespace rime
