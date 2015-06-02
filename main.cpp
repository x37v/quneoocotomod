#include "midicpp.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <fstream>
#include <limits>
#include <signal.h>
#include <yaml-cpp/yaml.h>

using std::cout;
using std::cerr;
using std::endl;
using std::string;

bool should_exit = false;
unsigned int last_pad = 0;

const uint8_t nrpn_chan = 15;
const uint16_t nrpn_center = (1 << 13);

std::shared_ptr<midicpp::Output> midiout;

void sighandler(int sig) {
  should_exit = true;
}

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
    pad_note_t() {
      for (int i = 0; i < 2; i++) {
        mCoarse.push_back(nrpn_center + nrpn_center / 2);
        mFine.push_back(0);
      }
    }
    uint16_t value(uint8_t index) const {
      return mCoarse[index] + mFine[index];
    }
    void coarse(uint8_t index, uint16_t v) { mCoarse[index] = v; }
    void fine(uint8_t index, int16_t v) { mFine[index] = v; }

    uint16_t coarse(uint8_t index) const { return mCoarse[index]; }
    int16_t fine(uint8_t index) const { return mFine[index]; }

    unsigned int num_notes() { return mCoarse.size(); }
  private:
    std::vector<uint16_t> mCoarse;
    std::vector<int16_t> mFine;
};

std::vector<pad_note_t> pad_notes(16);

void send_note(int pad_num) {
  midiout->nrpn(nrpn_chan, 0, pad_notes[pad_num].value(0));
  midiout->nrpn(nrpn_chan, 1, pad_notes[pad_num].value(1));
}

void pad(pad_t trig, int num, int val) {
  //cout << "pad: " << num << " " << to_string(trig) << "\t" << val << endl;

  switch (trig) {
    case P_NOTE_ON:
      last_pad = num;
      send_note(num);
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

void read_settings(std::string path) {
  {
    std::ifstream file(path);
    if (!file) {
      cerr << "no settings file at " << path << endl;
      return;
    }
  }

  YAML::Node yaml = YAML::LoadFile(path);
  if (yaml["pads"]) {
    for (int i = 0; i < std::min(yaml["pads"].size(), pad_notes.size()); i++) {
      for (int j = 0; j < yaml["pads"][i].size(); j++) {
        pad_notes[i].fine(yaml["pads"][i][j]["fine"].as<int16_t>());
        pad_notes[i].coarse(yaml["pads"][i][j]["coarse"].as<uint16_t>());
      }
    }
  }
}

void write_settings(std::string path) {
  std::ofstream file(path);
  YAML::Node yaml;

  YAML::Node pads;
  for (auto p: pad_notes) {
    YAML::Node notes;
    for (int i = 0; i < p.num_notes(); i++) {
      YAML::Node n;
      n["fine"] = p.fine(i);
      n["coarse"] = p.coarse(i);
      notes.push_back(n);
    }
    pads.push_back(notes);
  }
  yaml["pads"] = pads;
  file << yaml << endl;
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
  } else if (num < 4) {
    if (num % 2 == 0) {
      pad_notes[last_pad].coarse(num / 2, static_cast<uint16_t>(val) * 130);
    } else {
      pad_notes[last_pad].fine(num / 2, (static_cast<int16_t>(val) - 64) * 2);
    }
    send_note(last_pad);
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
  if (argc <= 1)
    read_settings("settings.yaml");
  else
    read_settings(argv[1]);
  
  signal(SIGINT, &sighandler);

  std::string quneo("QUNEO");
  std::string xnormidi("xnormidi");

  cout << "inputs: " << midicpp::Input::device_count() << endl;
  for (std::string n: midicpp::Input::device_list()) {
    if (n.find("QUNEO") != std::string::npos)
      quneo = n;
    cout << "\t" << n << endl;
  }

  cout << "outputs: " << midicpp::Output::device_count() << endl;
  for (std::string n: midicpp::Output::device_list()) {
    if (n.find("xnormidi") != std::string::npos)
      xnormidi = n;
    cout << "\t" << n << endl;
  }

  for (int i = 0; i < 16; i++) {
    pad_notes[i].coarse(i * 1000 + nrpn_center);
  }

  midicpp::Input in(quneo);
  midiout = std::make_shared<midicpp::Output>(xnormidi);

  in.with_message3(midicpp::CC, cc_cb);
  in.with_note(note_cb);

  //out.note(true, 0, 112, 127);
  //std::this_thread::sleep_for(std::chrono::milliseconds(500));
  //out.note(false, 0, 112, 0);
  midiout->nrpn(15, 0, (1 << 13));

  while (!should_exit) {
    in.process();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  write_settings("settings.yaml");

  return 0;
}

