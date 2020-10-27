#include "../include/network_flow.hpp"
#include "../include/time_expanded_network.hpp"
#include "../include/incremental_time_expanded_network.hpp"

const std::string NetworkFlow::SOLVER_NAME = "NetworkFlow";

NetworkFlow::NetworkFlow(Problem* _P) : Solver(_P)
{
  solver_name = NetworkFlow::SOLVER_NAME;
}

NetworkFlow::~NetworkFlow() {}

void NetworkFlow::run()
{
  auto network = IncrementalTimeExpandedNetwork(P);

  // simple test
  for (int t = 1; t <= max_timestep; ++t) {
    if (overCompTime()) break;

    info(" ", "elapsed:", getSolverElapsedTime(), ", makespan_limit:", t);

    network.update();

    // TimeExpandedNetwork network = TimeExpandedNetwork(P, t);
    // network.solve();

    if (network.isValid()) {
      solved = true;
      solution = network.getPlan();
      break;
    }

  }
}

void NetworkFlow::printHelp()
{
  std::cout << NetworkFlow::SOLVER_NAME << "\n"
            << "  (no option)" << std::endl;
}
