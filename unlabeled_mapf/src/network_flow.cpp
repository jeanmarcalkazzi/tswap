#include "../include/network_flow.hpp"
#include <fstream>

const std::string NetworkFlow::SOLVER_NAME = "NetworkFlow";

NetworkFlow::NetworkFlow(Problem* _P)
    : Solver(_P),
      use_incremental(true),
      use_filter(true),
      use_minimum_step(false),
      use_ilp_solver(false),
      use_binary_search(false),
      use_past_flow(true),
      minimum_step(1),
      is_optimal(false)
{
  solver_name = NetworkFlow::SOLVER_NAME;
}

NetworkFlow::~NetworkFlow() {}

void NetworkFlow::run()
{
  // setup minimum step
  if (use_minimum_step) {
    auto goals = P->getConfigGoal();
    for (auto s : P->getConfigStart()) {
      Node* g =
        *std::min_element(goals.begin(), goals.end(), [&](Node* v, Node* u) {
          return s->manhattanDist(v) < s->manhattanDist(u);
        });
      int d = s->manhattanDist(g);
      if (d > minimum_step) minimum_step = d;
    }
  }

  info(" ", "elapsed: ", getSolverElapsedTime(),
       ", minimum_step:", minimum_step);

  std::shared_ptr<TEN> network_flow;
  if (use_incremental) {
    network_flow = std::make_shared<TEN_INCREMENTAL>
      (P, minimum_step, use_filter, use_ilp_solver,
       max_comp_time - (int)getSolverElapsedTime());
  }

  // for binary search
  int lower_bound = 0;
  int upper_bound = -1;
  int t_binary = 1;

  int t_real = minimum_step;
  while (t_real <= max_timestep && !overCompTime()) {

    // build time expanded network
    if (!use_incremental) {
      network_flow = std::make_shared<TEN>(P, t_real, use_filter, use_ilp_solver);
    } else if (!use_past_flow) {
      network_flow->resetFlow();
    }

    // set time limit
    network_flow->setTimeLimit(max_comp_time - (int)getSolverElapsedTime());

    // update network
    network_flow->update(t_real);

    // updte log
    if (use_ilp_solver) {
#ifdef _GUROBI_
      HISTS.push_back({
          (int)getSolverElapsedTime(),
          t_real,
          network_flow->isValid(),
          0, 0,
          network_flow->getVariantsCnt(),
          network_flow->getConstraintsCnt()});
      info(" ", "elapsed:", getSolverElapsedTime(), ", makespan_limit:", t_real,
           ", valid:", network_flow->isValid(),
           ", variants:", network_flow->getVariantsCnt(),
           ", constraints:", network_flow->getConstraintsCnt());
#endif
    } else {
      HISTS.push_back({
          (int)getSolverElapsedTime(),
          t_real,
          network_flow->isValid(),
          network_flow->getDfsCnt(),
          network_flow->getNodesNum(),
          0, 0});
      float visited_rate =
        (float)network_flow->getDfsCnt() / network_flow->getNodesNum();
      info(" ", "elapsed:", getSolverElapsedTime(), ", makespan_limit:", t_real,
           ", valid:", network_flow->isValid(),
           ", visited_ndoes:", network_flow->getDfsCnt(), "/",
           network_flow->getNodesNum(), "=", visited_rate);
    }

    // check solution
    if (network_flow->isValid()) {
      solved = true;
      solution = network_flow->getPlan();
      if (!use_binary_search) {
        is_optimal = true;
        break;
      }
      upper_bound = t_binary;
    } else {
      lower_bound = t_binary;
    }

    // update timestep
    if (!use_binary_search) {
      ++t_real;
    } else {
      t_binary = (upper_bound == -1)
        ? t_binary * 2
        : (upper_bound - lower_bound) / 2 + lower_bound;
      if (t_binary == lower_bound) {
        is_optimal = true;
        break;
      }
      t_real = t_binary + minimum_step - 1;  // for binary search
    }
  }
}

void NetworkFlow::setParams(int argc, char* argv[])
{
  struct option longopts[] = {
      {"no-cache", no_argument, 0, 'n'},
      {"no-past-flow", no_argument, 0, 'p'},
      {"no-filter", no_argument, 0, 'f'},
      {"use-minimum-step", no_argument, 0, 'm'},
      {"use-ilp-solver", no_argument, 0, 'g'},
      {"start-timestep", no_argument, 0, 't'},
      {"use-binary-search", no_argument, 0, 'b'},
      {0, 0, 0, 0},
  };
  optind = 1;  // reset
  int opt, longindex;
  while ((opt = getopt_long(argc, argv, "nfmgpt:b", longopts, &longindex)) !=
         -1) {
    switch (opt) {
      case 'n':
        use_incremental = false;
        break;
      case 'p':
        use_past_flow = false;
        break;
      case 'f':
        use_filter = false;
        break;
      case 'm':
        use_minimum_step = true;
        break;
      case 'b':
        use_binary_search = true;
        break;
      case 't':
        minimum_step = std::atoi(optarg);
        if (minimum_step <= 0) {
          minimum_step = 1;
          warn("start timestep should be greater than 0");
        }
        break;
#ifdef _GUROBI_
      case 'g':
        use_ilp_solver = true;
        break;
#endif
      default:
        break;
    }
  }
}

void NetworkFlow::printHelp()
{
  std::cout << NetworkFlow::SOLVER_NAME << "\n"
            << "  -n --no-cache"
            << "                 "
            << "implement without cache\n"

            << "  -p --no-past-flow"
            << "             "
            << "implement without past flow\n"

            << "  -f --no-filter"
            << "                "
            << "implement without filter\n"

            << "  -m --use-minimum-step"
            << "         "
            << "implement with minimum-step (Manhattan distance)\n"

            << "  -b --use-binary-search"
            << "        "
            << "implement with binary search (and without cache)\n"

            << "  -t --start-timestep [INT]"
            << "     "
            << "start timestep";

#ifdef _GUROBI_
  std::cout << "\n  -g --use-ilp-solver"
            << "           "
            << "implement with ILP solver (GUROBI)";
#endif

  std::cout << std::endl;
}

void NetworkFlow::makeLog(const std::string& logfile)
{
  std::ofstream log;
  log.open(logfile, std::ios::out);
  makeLogBasicInfo(log);

  log << "params="
      << "\nuse_incremental:" << use_incremental
      << "\nuse_filter:" << use_filter
      << "\nuse_minimum_step:" << use_minimum_step
      << "\nuse_ilp_solver:" << use_ilp_solver
      << "\nuse_binary_search:" << use_binary_search
      << "\nuse_past_flow:" << use_past_flow
      << "\nminimum_step:" << minimum_step
      << "\n";
  log << "optimal=" << is_optimal << "\n";
  log << "history=\n";
  for (auto hist : HISTS) {
    log << "elapsed:" << hist.elapsed
        << ",makespan:" << hist.makespan
        << ",valid:" << hist.valid
        << ",network_size:" << hist.network_size
        << ",visited:" << hist.visited_nodes;
#ifdef _GUROBI_
    if (use_ilp_solver) {
      log << ",variants:" << hist.variants_cnt
          << ",constraints:" << hist.constraints_cnt;
    }
#endif
    log << "\n";
  }

  makeLogSolution(log);
  log.close();
}
