/*
 * DYNAMIC TRAFFIC SIGNAL OPTIMIZATION SYSTEM
 * Build : g++ main.cpp -o main
 * Run   : .\main.exe
 */

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
void sleepMs(int ms) { Sleep(ms); }
void enableColors() {
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  GetConsoleMode(h, &mode);
  SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
#include <unistd.h>
void sleepMs(int ms) { usleep(ms * 1000); }
void enableColors() {}
#endif

int clamp(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

const std::string RST = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string GRN = "\033[32m";
const std::string YLW = "\033[33m";
const std::string BLU = "\033[34m";
const std::string MAG = "\033[35m";
const std::string CYN = "\033[36m";
const std::string BGGRN = "\033[42m";
const std::string BGRED = "\033[41m";
const std::string BGYLW = "\033[43m";
const std::string CLRSCR = "\033[2J\033[H";

const int NUM_ROADS = 4;
const int T_CYCLE = 60;
const int MIN_GREEN = 5;
const int MAX_GREEN = 45;
const int YELLOW_SEC = 2;
const int THROUGHPUT =
    2; // vehicles cleared per second of green (comparison model)

enum class Sig { R, Y, G };

struct Road {
  std::string name;
  int vehicles = 0;
  int originalInput = 0;
  Sig signal = Sig::R;
  int totalCleared = 0;
};

struct SchedEntry {
  int roadIdx, vehicles, greenTime, rank;
  // Max-heap comparator: road with MOST vehicles has highest priority
  // operator< is reversed because std::priority_queue is a max-heap
  bool operator<(const SchedEntry &o) const { return vehicles < o.vehicles; }
};

// Simulated OpenCV MOG2 detector
// Phase 3: replaced by real MOG2 background subtraction
// Phase 2: vehicle counts stay exactly as entered by user
class SimDetector {
public:
  int detect(int cur) { return cur; }
};

// ── Greedy Scheduler — DAA Core ──────────────────────────────────────────────
// Strategy : Greedy — always serve the road with the most vehicles first
// Structure: Max-Heap (std::priority_queue) for O(log n) greedy selection
// Formula  : T_i = (V_i / V_total) * T_CYCLE   clamped to [MIN_GREEN,
// MAX_GREEN] Complexity: O(n log n) — n heap insertions + n extract-max
// operations
class GreedyScheduler {
public:
  static std::vector<SchedEntry> build(const std::vector<Road> &roads) {
    int total = 0;
    for (int i = 0; i < (int)roads.size(); i++)
      total += roads[i].vehicles;
    if (total == 0)
      total = 1;

    // ── Priority Queue (Max-Heap) ─────────────────────────────────────
    // Push all roads into a max-heap keyed by vehicle count.
    // The greedy choice — "pick the busiest road" — is an O(log n)
    // extract-max from the heap, vs O(n) scan of an unsorted list.
    std::priority_queue<SchedEntry> pq;
    for (int i = 0; i < (int)roads.size(); i++) {
      int vi = roads[i].vehicles;
      int t = (int)((float)vi / total * T_CYCLE + 0.5f);
      t = clamp(t, MIN_GREEN, MAX_GREEN);
      SchedEntry e;
      e.roadIdx = i;
      e.vehicles = vi;
      e.greenTime = t;
      e.rank = 0; // rank assigned during extraction
      pq.push(e); // O(log n) heap insert
    }

    // ── Greedy extraction — highest-priority road comes out first ─────
    std::vector<SchedEntry> sched;
    int rank = 0;
    while (!pq.empty()) {
      SchedEntry top = pq.top(); // O(1)  peek max
      pq.pop();                  // O(log n) extract max
      top.rank = rank++;
      sched.push_back(top);
    }
    return sched;
  }

  // Green-time utilisation: % of total green seconds given to roads with
  // vehicles
  static int efficiency(const std::vector<Road> &roads,
                        const std::vector<SchedEntry> &sched) {
    if (sched.empty())
      return 0;
    int usefulTime = 0, totalTime = 0;
    for (int i = 0; i < (int)sched.size(); i++) {
      totalTime += sched[i].greenTime;
      if (roads[sched[i].roadIdx].vehicles > 0)
        usefulTime += sched[i].greenTime;
    }
    return totalTime > 0 ? usefulTime * 100 / totalTime : 100;
  }
};

// ── Display helpers
// ───────────────────────────────────────────────────────────
std::string badge(Sig s) {
  if (s == Sig::G)
    return BGGRN + BOLD + " G " + RST;
  if (s == Sig::Y)
    return BGYLW + BOLD + " Y " + RST;
  return BGRED + BOLD + " R " + RST;
}

std::string bar(int v, int mx, int w, std::string col) {
  int f = mx > 0 ? v * w / mx : 0;
  std::string b = col + "[";
  for (int i = 0; i < w; i++)
    b += (i < f ? "#" : ".");
  return b + "]" + RST;
}

std::string tbar(int cur, int tot) {
  int w = 14, f = tot > 0 ? cur * w / tot : 0;
  std::string b = GRN + "[";
  for (int i = 0; i < w; i++)
    b += (i < f ? "=" : ".");
  return b + "] " + std::to_string(cur) + "s" + RST;
}

void printHeader() {
  std::cout << BOLD + CYN
            << "+----------------------------------------------------------+\n"
               "|   DYNAMIC TRAFFIC SIGNAL OPTIMIZATION SYSTEM            |\n"
               "|   DAA: Greedy Scheduling  |  OpenCV: MOG2  |  C++      |\n"
               "+----------------------------------------------------------+\n"
            << RST;
}

void printView(const std::vector<Road> &roads,
               const std::vector<SchedEntry> &sched, int pi, int timer,
               int cycle, int cleared) {
  std::cout << "\n"
            << "                " << BOLD << roads[0].name << RST << "  "
            << badge(roads[0].signal) << "  " << roads[0].vehicles << "v\n"
            << "                    |\n"
            << " " << BOLD << roads[3].name << RST << " "
            << badge(roads[3].signal) << " " << roads[3].vehicles << "v"
            << "  ----[+]----  " << roads[2].name << " "
            << badge(roads[2].signal) << " " << roads[2].vehicles << "v\n"
            << "                    |\n"
            << "                " << BOLD << roads[1].name << RST << "  "
            << badge(roads[1].signal) << "  " << roads[1].vehicles << "v\n\n";

  std::cout << BOLD << "  Cycle: " << YLW << cycle << RST << BOLD
            << "   Cleared: " << GRN << cleared << RST << BOLD
            << "   Efficiency: " << CYN
            << GreedyScheduler::efficiency(roads, sched) << "%" << RST
            << "  (green secs on active roads)\n\n";

  std::cout << BOLD << "  GREEDY SIGNAL QUEUE:\n"
            << RST
            << "  +----+----------+----------+----------+------------------+\n"
            << "  | #  | Road     | Vehicles | Green(s) | Load             |\n"
            << "  +----+----------+----------+----------+------------------+\n";

  for (int r = 0; r < (int)sched.size(); r++) {
    const SchedEntry &s = sched[r];
    bool act = (r == pi % (int)sched.size());
    std::string p = act ? BGGRN + BOLD : "";
    std::string e = act ? RST : "";
    std::string t = act ? (GRN + std::to_string(timer) + "s" + RST)
                        : (std::to_string(s.greenTime) + "s");
    std::cout << "  | " << p << std::setw(2) << (r + 1) << e << " | " << p
              << std::left << std::setw(8) << roads[s.roadIdx].name << e
              << " |    " << p << std::right << std::setw(3) << s.vehicles
              << "     " << e << "| " << std::setw(4) << t << "    "
              << "| " << bar(s.vehicles, 80, 14, act ? GRN : BLU) << "   |\n";
  }
  std::cout
      << "  +----+----------+----------+----------+------------------+\n\n";

  if (!sched.empty()) {
    const SchedEntry &cur = sched[pi % sched.size()];
    std::string ph = (roads[cur.roadIdx].signal == Sig::Y)
                         ? YLW + "YELLOW" + RST
                         : GRN + "GREEN" + RST;
    std::cout << "  Active: " << BOLD << roads[cur.roadIdx].name << RST
              << "  | " << ph << "  " << tbar(timer, cur.greenTime) << "\n";
  }

  std::cout << "\n  " << CYN << "[OpenCV MOG2 simulated] " << RST << "Counts: ";
  for (int i = 0; i < (int)roads.size(); i++)
    std::cout << roads[i].name << ":" << roads[i].vehicles << " ";
  std::cout << "\n";
}

void printSched(const std::vector<Road> &roads,
                const std::vector<SchedEntry> &sched, int cycle) {
  int total = 0;
  for (int i = 0; i < (int)roads.size(); i++)
    total += roads[i].vehicles;

  std::cout << "\n"
            << BOLD + MAG << "  === GREEDY SCHEDULE (Cycle " << cycle
            << ") ===\n"
            << RST << MAG << "  T_i = (V_i / " << total << ") x " << T_CYCLE
            << "s  clamp[" << MIN_GREEN << "," << MAX_GREEN << "]\n"
            << RST;

  for (int i = 0; i < (int)sched.size(); i++) {
    const SchedEntry &s = sched[i];
    float pct = (float)s.vehicles / total * 100.0f;
    std::cout << "  #" << (s.rank + 1) << " " << BOLD << std::left
              << std::setw(8) << roads[s.roadIdx].name << RST << " | "
              << std::right << std::setw(2) << s.vehicles << "v (" << std::fixed
              << std::setprecision(1) << pct << "%)"
              << " | " << GRN << std::setw(2) << s.greenTime << "s" << RST
              << " " << bar(s.greenTime, MAX_GREEN, 12, YLW) << "\n";
  }
  std::cout << "\n";
  sleepMs(1500);
}

// ── Traffic System
// ────────────────────────────────────────────────────────────
class TrafficSystem {
  std::vector<Road> roads;
  std::vector<SchedEntry> schedule;
  SimDetector det;
  int pi = 0, timer = 0, cycle = 1, cleared = 0;
  bool yPhase = false, done = false;
  int yTimer = 0;
  int MAX_CYCLES = 3; // FIX 3: No longer const — user-configurable at runtime

  void allRed() {
    for (int i = 0; i < (int)roads.size(); i++)
      roads[i].signal = Sig::R;
  }

  void rebuild() {
    if (cycle > MAX_CYCLES) {
      done = true;
      return;
    }
    schedule = GreedyScheduler::build(roads);
    if (!schedule.empty()) {
      timer = schedule[0].greenTime;
      allRed();
      roads[schedule[0].roadIdx].signal = Sig::G;
    }
    printSched(roads, schedule, cycle);
  }

  void discharge(int idx) {
    // FIX 2: Use float before casting to int to avoid truncation losing the last car
    float frac = roads[idx].vehicles * 0.70f;
    int n = (int)(frac + 0.5f); // round instead of truncate
    if (n < 1 && roads[idx].vehicles > 0)
      n = 1; // always clear at least 1 car
    if (n > roads[idx].vehicles)
      n = roads[idx].vehicles;
    roads[idx].vehicles -= n;
    roads[idx].totalCleared += n;
    cleared += n;

    std::cout << "\n  " << GRN << ">> " << roads[idx].name
              << " phase done: " << n << " vehicles cleared, "
              << roads[idx].vehicles << " remaining" << RST << "\n";
    sleepMs(800);
  }

public:
  TrafficSystem() {
    printHeader();
    std::string names[4] = {"North", "South", "East", "West"};
    std::cout << "\n  Enter number of vehicles on each road (0-80):\n";
    for (int i = 0; i < 4; i++) {
      Road r;
      r.name = names[i];
      int v = -1;
      while (v < 0 || v > 80) {
        std::cout << "  " << names[i] << " road : ";
        std::cin >> v;
        if (v < 0 || v > 80)
          std::cout << "  Enter 0-80.\n";
      }
      r.vehicles = v;
      r.originalInput = v;
      roads.push_back(r);
    }
    std::cin.ignore();

    // FIX 1: Warn if all roads have zero vehicles — simulation will be trivial
    int totalInput = 0;
    for (int i = 0; i < 4; i++) totalInput += roads[i].vehicles;
    if (totalInput == 0) {
      std::cout << YLW << "  WARNING: All roads have 0 vehicles. "
                << "Simulation will run with minimum green times only.\n" << RST;
    }

    // FIX 3: Let user choose number of cycles (default 3)
    int userCycles = 0;
    while (userCycles < 1 || userCycles > 10) {
      std::cout << "  Number of simulation cycles (1-10, default 3): ";
      std::string line;
      std::getline(std::cin, line);
      if (line.empty()) { userCycles = 3; break; }
      try { userCycles = std::stoi(line); } catch (...) { userCycles = 0; }
      if (userCycles < 1 || userCycles > 10)
        std::cout << "  Please enter 1-10.\n";
    }
    const_cast<int&>(MAX_CYCLES) = userCycles;

    std::cout << "\n"
              << GRN << "  Starting simulation (" << MAX_CYCLES
              << " cycles)...\n"
              << RST;
    sleepMs(1000);
    rebuild();
  }

  void tick() {
    if (done)
      return;

    for (int i = 0; i < (int)roads.size(); i++)
      roads[i].vehicles = det.detect(roads[i].vehicles);

    if (yPhase) {
      --yTimer;
      allRed();
      roads[schedule[pi % schedule.size()].roadIdx].signal = Sig::Y;
      timer = yTimer;
      if (yTimer <= 0) {
        yPhase = false;
        pi = (pi + 1) % (int)schedule.size();
        if (pi == 0) {
          ++cycle;
          rebuild();
          return;
        }
        allRed();
        roads[schedule[pi].roadIdx].signal = Sig::G;
        timer = schedule[pi].greenTime;
      }
      return;
    }

    --timer;
    if (timer <= 0) {
      discharge(schedule[pi].roadIdx);
      yPhase = true;
      yTimer = YELLOW_SEC;
    }
  }

  bool isDone() { return done; }

  void render() {
    std::cout << CLRSCR;
    printHeader();
    printView(roads, schedule, pi, timer, cycle, cleared);
  }

  void printFinalReport() {
    std::cout << "\n"
              << BOLD + GRN
              << "  ============================================\n"
              << "   SIMULATION COMPLETE -- " << MAX_CYCLES << " cycles done\n"
              << "  ============================================\n"
              << RST;

    std::cout << BOLD << "  Total vehicles cleared : " << GRN << cleared << RST
              << "\n";
    std::cout << BOLD << "  Green-time efficiency  : " << CYN
              << GreedyScheduler::efficiency(roads, schedule) << "%" << RST
              << "  (green secs on roads with vehicles)\n\n";

    // ── Per-road summary ──────────────────────────────────────────────────
    std::cout << BOLD << "  Per-road summary:\n" << RST;
    std::cout << "  +----------+---------------+----------+-----------+\n";
    std::cout << "  | Road     | Input vehicles| Cleared  | Remaining |\n";
    std::cout << "  +----------+---------------+----------+-----------+\n";
    for (int i = 0; i < (int)roads.size(); i++) {
      std::cout << "  | " << std::left << std::setw(8) << roads[i].name << " | "
                << std::right << std::setw(13) << roads[i].originalInput
                << " | " << std::setw(8) << roads[i].totalCleared << " | "
                << std::setw(9) << roads[i].vehicles << " |\n";
    }
    std::cout << "  +----------+---------------+----------+-----------+\n";

    // ── Greedy vs Fixed-time comparison (using original vehicle counts) ───
    const int fixedT = clamp(T_CYCLE / NUM_ROADS, MIN_GREEN, MAX_GREEN);

    // Rebuild greedy schedule from original inputs for a fair comparison
    std::vector<Road> tmpR = roads;
    for (int i = 0; i < (int)tmpR.size(); i++)
      tmpR[i].vehicles = tmpR[i].originalInput;
    std::vector<SchedEntry> cmpSched = GreedyScheduler::build(tmpR);
    std::vector<int> gGreen(NUM_ROADS, fixedT);
    for (int i = 0; i < (int)cmpSched.size(); i++)
      gGreen[cmpSched[i].roadIdx] = cmpSched[i].greenTime;

    std::cout
        << "\n"
        << BOLD + MAG
        << "  === GREEDY vs FIXED-TIME (per-cycle, initial load) ===\n"
        << RST << "  Fixed-time  : every road gets " << fixedT
        << "s  (= T_cycle / " << NUM_ROADS << ")\n"
        << "  Throughput  : " << THROUGHPUT << " vehicles/sec of green\n\n"
        << "  +----------+------+---------------+---------------+----------+\n"
        << "  | Road     | Veh  | Fixed         | Greedy        |  Gain    |\n"
        << "  +----------+------+---------------+---------------+----------+\n";

    int totG = 0, totF = 0;
    for (int i = 0; i < NUM_ROADS; i++) {
      int v = roads[i].originalInput;
      int gT = gGreen[i];
      int gC = (v < gT * THROUGHPUT) ? v : gT * THROUGHPUT;
      int fC = (v < fixedT * THROUGHPUT) ? v : fixedT * THROUGHPUT;
      int gain = gC - fC;
      std::string gainStr = (gain >= 0 ? "+" : "") + std::to_string(gain) + "v";
      std::string gainCol = gain > 0 ? GRN : (gain < 0 ? YLW : RST);
      std::cout << "  | " << std::left << std::setw(8) << roads[i].name << " | "
                << std::right << std::setw(3) << v << "v "
                << " | " << fixedT << "s -> " << std::setw(3) << fC
                << "v cleared "
                << " | " << gT << "s -> " << std::setw(3) << gC << "v cleared "
                << " | " << gainCol << std::setw(5) << gainStr << RST
                << "   |\n";
      totG += gC;
      totF += fC;
    }
    int impPct = totF > 0 ? (totG - totF) * 100 / totF : 0;
    std::cout
        << "  +----------+------+---------------+---------------+----------+\n"
        << "  Greedy: " << GRN << BOLD << totG << "v/cycle" << RST
        << "   Fixed: " << YLW << totF << "v/cycle" << RST << "   "
        << BOLD + GRN << "Improvement: +" << impPct << "%\n"
        << RST;
  }

  void run() {
    while (!isDone()) {
      tick();
      if (!isDone()) {
        render();
        sleepMs(300);
      }
    }
    printFinalReport();
  }
};

int main(int argc, char *argv[]) {
  enableColors();
  TrafficSystem sys;
  sys.run();
  return 0;
}