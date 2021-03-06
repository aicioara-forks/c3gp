#include "tsp_solver.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

void TspSolver::setNumberOfNodes(int nodes) {
    totalNodes = nodes;
    adjacencyMatrix = new double*[nodes];
    for (int i = 0; i < nodes; i++) {
        adjacencyMatrix[i] = new double[nodes];
    }
}

void TspSolver::addPoint(int firstNode, int secondNode, double cost) {
    if (totalNodes < 0) {
        throw std::runtime_error("Number of nodes in graph not initialized!");
    }

    if (firstNode >= totalNodes ||
        secondNode >= totalNodes ||
        firstNode < 0 ||
        secondNode < 0) {
        throw std::out_of_range("Node indices out of range!");
    }

    adjacencyMatrix[firstNode][secondNode] = cost;
}

void TspSolver::setStartingPoint(int node) {
    startingPoint = node;
}

void TspSolver::setGroupForNode(int node, int group) {
    throw std::runtime_error("There are not groups to set. Use GTSP solver for that");
}

std::vector<int>* TspSolver::solve() {
    checkBuildReady();

    if (totalNodes < 8) {
        return solveTspWithBacktracking();
    }

    if (totalNodes < 20) {
        return solveTspWithDynamicProgramming();
    }

    std::vector<int>* result = solveTspWithNNGreedy();
    apply2OptLocalSearch(result);

    return result;
}

std::vector<int>* TspSolver::solveTspWithNNGreedy() {

    checkBuildReady();

    std::vector<int>* result = new std::vector<int>();

    int pivot = startingPoint;
    bool visited[totalNodes];
    for (int i = 0; i < totalNodes; i++) {
        visited[i] = 0;
    }

    result->push_back(0);
    while (result->size() < totalNodes) {
        visited[pivot] = 1;

        double min_dist = std::numeric_limits<double>::max();
        int next = 0;

        for (int i = 0; i < totalNodes; i++) {
            if (!visited[i] && adjacencyMatrix[pivot][i] < min_dist) {
                min_dist = adjacencyMatrix[pivot][i];
                next = i;
            }
        }

        result->push_back(next);
        pivot = next;
    }

    return result;
}

double TspSolver::computeTourWeight(std::vector<int>* tour) {
    if (tour->empty()) {
        return 0;
    }

    double weight = 0;
    for (int i = 1; i < tour->size(); ++i) {
        weight += adjacencyMatrix[tour->at(i-1)][tour->at(i)];
    }
    weight += adjacencyMatrix[tour->at(tour->size() - 1)][startingPoint];

    return weight;
}

// Applies 2-opt local search heuristic to the given tour, modifying it
// in-place.
void TspSolver::apply2OptLocalSearch(std::vector<int>* tour) {
    bool hasImprovement = true;
    double min_dist = computeTourWeight(tour);
    std::vector<int> temp = *tour; // copy tour to a temporary buffer
    temp.push_back(startingPoint); // makes implementation easier
    totalNodes++;

    while (hasImprovement) {
        hasImprovement = false;
        for (int i = 1; i < totalNodes; i++) {
            for (int j = i + 2; j < totalNodes; j++) {
                // We are trying to cut (i-1, i) and (j-1, j).
                // We're then comparing cost of start->(i-1), (i-1)->(j-1),
                // (j-1)->(i), (i)->(j) and (j)->start vs. original.
                double new_dist = 0;

                // Start to (i - 1)
                for (int k = 1; k < i; ++k) {
                    new_dist += adjacencyMatrix[temp[k-1]][temp[k]];
                }

                // (i - 1) to (j - 1)
                new_dist += adjacencyMatrix[temp[i-1]][temp[j-1]];

                // (j - 1) to (i), backwards
                for (int k = j - 1; k > i; --k) {
                    new_dist += adjacencyMatrix[temp[k]][temp[k-1]];
                }

                // (i) to (j)
                new_dist += adjacencyMatrix[temp[i]][temp[j]];

                // (j) to start
                for (int k = j + 1; k < totalNodes; k++) {
                    new_dist += adjacencyMatrix[temp[k-1]][temp[k]];
                }

                // Check if there is an improvement
                if (new_dist < min_dist) {
                    hasImprovement = true;
                    min_dist = new_dist;
                    // Swap the relevant elements.
                    std::reverse(temp.begin() + i, temp.begin() + j);
                }
            }
        }
    }
    totalNodes--;
    temp.pop_back(); // get rid of the extra origin added to make things easier
    *tour = temp; // update the starting vector.
}

static std::vector<int> tbOrderedResult;
static std::vector<int> tbAccumulator;
static double tbCurrent;
static double tbMinimum;
static int tbTotalVisited;
static int* tbVisited;

void TspSolver::tbBkt(int currNode) {

    tbTotalVisited ++;
    tbAccumulator.push_back(currNode);
    tbVisited[currNode] = 1;

    if (tbTotalVisited == totalNodes) {
        if (tbCurrent + adjacencyMatrix[currNode][startingPoint] < tbMinimum) {
            tbMinimum = tbCurrent + adjacencyMatrix[currNode][startingPoint];
            tbOrderedResult.clear();
            for (const auto& it : tbAccumulator) {
                tbOrderedResult.push_back(it);
            }
        }
    }

    for (int i = 0; i < totalNodes; i++) {
        if (! tbVisited[i]) {
            tbCurrent += adjacencyMatrix[currNode][i];
            tbBkt(i);
            tbCurrent -= adjacencyMatrix[currNode][i];
        }
    }

    tbVisited[currNode] = 0;
    tbAccumulator.pop_back();
    tbTotalVisited --;
}

std::vector<int>* TspSolver::solveTspWithBacktracking() {

    checkBuildReady();

    tbMinimum = std::numeric_limits<double>::max();
    tbOrderedResult.clear();
    tbAccumulator.clear();
    tbVisited = new int[totalNodes];
    for (int i = 0; i < totalNodes; i++) {
        tbVisited[i] = 0;
    }
    tbCurrent = 0;
    tbTotalVisited = 0;

    tbBkt(startingPoint);

    return &tbOrderedResult;
}

// Dynamic Programming code - implementation of Held-Karp algorithm.
// Eventually, this was implemented using bitwise operations. I acknowledge
// there's a tradeoff for speed vs readability here (the code's not the most
// understandable).

// Effectively a double-dimensional array, though allocated as 1-dimensional
// for memory locality. Indexed by bitset * totalNodes + endpoint, where
// bitset is an integer where the ith bit is set iff node i is already visited
// by this path, and endpoint indicates the ending point of this path.
// opt_map[bitset * totalNodes + endpoint] contains the path of minimum cost
// from the origin through every node in the bitset, ending at endpoint.
static double* opt_map;

// prev_step is indexed similarly to opt_map.
// prev_step[bitset * totalNodes + endpoint] contains the last node visited
// before the endpoint on the optimal route.
static int* prev_step;

double TspSolver::dynamicSolve(int visited, int end) {
    int row_size = totalNodes;

    // memoisation: if we have already considered this subproblem, we retrieve
    // its solution from the opt_map.
    if (opt_map[visited * row_size + end] != -1) {
        return opt_map[visited * row_size + end];
    }

    // base case: nothing visited
    if (visited == 0) {
        return 0;
    }

    // base case: 1 node visited - the condition checks if it's a power of 2
    if ((visited & (visited - 1)) == 0) {
        opt_map[visited * row_size + end]
            = adjacencyMatrix[startingPoint][end];
        prev_step[visited * row_size + end]
            = startingPoint;
        return adjacencyMatrix[startingPoint][end];
    }

    // The 'other' cases when we need to take something out
    double min_cost = std::numeric_limits<double>::max();
    
    // remove end from the visited list
    int visited_before = visited & ~(1 << end); 
    for (int i = 0; i < totalNodes; ++i) {
        if (visited_before & (1 << i)) {
            double candidate_cost = dynamicSolve(visited_before, i) +
                                    adjacencyMatrix[i][end];
            if (candidate_cost < min_cost) {
                min_cost = candidate_cost;
                prev_step[visited_before * row_size + end] = i;
            }
        }
    }
    opt_map[visited * row_size + end] = min_cost;
    return opt_map[visited * row_size + end];
}

std::vector<int>* TspSolver::solveTspWithDynamicProgramming() {
    int num_sets = 1 << (totalNodes);
    opt_map = new double[num_sets * totalNodes];
    prev_step = new int[num_sets * totalNodes];
    for(int i = 0; i < num_sets * totalNodes; ++i) {
        opt_map[i] = -1;
    }

    int all_nodes = (1 << (totalNodes)) - 1;
    // -2, to get all 1s except for the units position.
    dynamicSolve(all_nodes, 0);

    std::vector<int>* solution = new std::vector<int>();
    int considered_nodes = all_nodes;
    int currNode = startingPoint;
    for(int i = 0; i < totalNodes; ++i) {
        currNode = prev_step[considered_nodes * totalNodes + currNode];
        considered_nodes = considered_nodes & ~(1 << currNode);
        solution->push_back(currNode);
    }
    return solution;
}

//GENETIC ALGORITHM CODE
// TODO: inspired from here
// https://github.com/parano/GeneticAlgorithm-TSP/blob/master/src/algorithm.js

std::vector<int>* TspSolver::solveTspWithGeneticAlgorithm() {

    throw std::runtime_error("TODO solve TSP using Genetic Algorithms");

    checkBuildReady();

    std::vector<int>* result = new std::vector<int>();

    return result;
}
