#include "midicpp.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>

using std::cout;
using std::endl;
using std::string;

const uint8_t nrpn_chan = 15;
const uint16_t nrpn_center = (1 << 13);

std::shared_ptr<midicpp::Output> midiout;

int pad_rm(int num) {
  return (num % 4) + 4 * (3 - num / 4);
}

enum pad_t {
  P_NOTE_ON,
  P_NOTE_OFF,
  P_PRESSURE,
  P_X,
  P_Y
};

string to_string(pad_t t) {
  switch (t) {
    case P_NOTE_ON: return "n_on";
    case P_NOTE_OFF: return "n_off";
    case P_PRESSURE: return "press";
    case P_X: return "x";
    case P_Y: return "y";
  }
}

class pad_note_t {
  public:
    uint16_t value() const {
      return mCoarse + mFine;
    }
    void coarse(uint16_t v) { mCoarse = v; }
    void fine(int16_t v) { mFine = v; }
  private:
    uint16_t mCoarse = nrpn_center + nrpn_center / 2;
    int16_t mFine = 0;
};

pad_note_t pad_notes[16];

void pad(pad_t trig, int num, int val) {
  //cout << "pad: " << num << " " << to_string(trig) << "\t" << val << endl;

  switch (trig) {
    case P_NOTE_ON:
      midiout->nrpn(nrpn_chan, 0, pad_notes[num].value());
      midiout->nrpn(nrpn_chan, 7, nrpn_center + nrpn_center * (val / 127.0));
      break;
    case P_PRESSURE:
      midiout->nrpn(nrpn_chan, 6, nrpn_center + nrpn_center * (val / 127.0));
      break;
    case P_X:
      if (val != 63)
        midiout->nrpn(nrpn_chan, 4, nrpn_center + nrpn_center * (val / 127.0 - 0.5));
      break;
    case P_Y:
      if (val != 63)
        midiout->nrpn(nrpn_chan, 5, nrpn_center + nrpn_center * (val / 127.0 - 0.5));
      break;
    default:
      break;
  }
}

void cc_cb(uint8_t chan, uint8_t num, uint8_t val) {
  if (chan != 0)
    return;
  //cout << "cc " << (int)chan << " " << (int)num << " " << (int)val << endl;
  if (num >= 23 && num <= 70) { //pads
    int index = num - 23;
    int p = pad_rm(index / 3);
    pad_t t;
    switch (index % 3) {
      case 0: t = P_PRESSURE; break;
      case 1: t = P_X; break;
      case 2: t = P_Y; break;
    }
    pad(t, p, val);
    return;
  } else if (num == 10) {  //xfade high on right
    midiout->nrpn(nrpn_chan, 3, nrpn_center + nrpn_center * (val / 127.0));
    return;
  }
}

void note_cb(bool on, uint8_t chan, uint8_t num, uint8_t vel) {
  if (chan != 0)
    return;
  if (num >= 36 && num <= 51) {
    int p = pad_rm(num - 36);
    pad(on ? P_NOTE_ON : P_NOTE_OFF, p, vel);
    return;
  }
  cout << "note " << (int)chan << " " << (int)num << " " << (int)vel << endl;
}

int main(int argc, char *argv[]) {
  cout << "inputs: " << midicpp::Input::device_count() << endl;
  for (std::string n: midicpp::Input::device_list())
    cout << "\t" << n << endl;

  cout << "outputs: " << midicpp::Output::device_count() << endl;
  for (std::string n: midicpp::Output::device_list())
    cout << "\t" << n << endl;

  for (int i = 0; i < 16; i++) {
    pad_notes[i].coarse(i * 1000 + nrpn_center);
  }

  midicpp::Input in("QUNEO 28:0");
  midiout = std::make_shared<midicpp::Output>("xnormidi:0");
  //midicpp::Output out("Midi Through:0");

  in.with_message3(midicpp::CC, cc_cb);
  in.with_note(note_cb);

  //out.note(true, 0, 112, 127);
  //std::this_thread::sleep_for(std::chrono::milliseconds(500));
  //out.note(false, 0, 112, 0);
  midiout->nrpn(15, 0, (1 << 13));

  while (1) {
    in.process();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return 0;
}

