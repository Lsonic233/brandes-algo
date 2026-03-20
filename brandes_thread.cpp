#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sys/resource.h>
#include <thread>
#include <utility>
#include <vector>

using namespace std;

class Centrality {
  private:
    int vertices;
    vector<vector<int>> adj;
    vector<double> centrality;
    mutex centrality_mutex;

  public:
    // Constructor to initialize the graph's size
    Centrality(int v) : vertices(v), adj(v), centrality(v, 0.0) {}

    // Adds an edge for an UNDIRECTED graph
    void addEdge(int u, int v) {
        adj[u].push_back(v);
        adj[v].push_back(u);
    }

    void calculateBrandes() {
        // Use threads
        unsigned int num_threads = thread::hardware_concurrency();
        if (num_threads == 0)
            num_threads = 4;

        vector<thread> threads;
        atomic<int> current_source(0);

        auto worker = [&](int thread_id) {
            // Thread-private data structures
            vector<vector<int>> P(vertices);
            vector<double> sigma(vertices, 0.0);
            vector<int> d(vertices, -1);
            vector<double> delta(vertices, 0.0);
            // queue<int> Q;
            // stack<int> S;
            vector<int> Q(vertices);
            vector<int> S(vertices);
            int q_head = 0, q_tail = 0, s_top = 0;

            // Thread-local accumulation buffer
            vector<double> local_centrality(vertices, 0.0);

            while (true) {
                // Fetch next task atomically
                int s = current_source.fetch_add(1);
                if (s >= vertices)
                    break;
                if (adj[s].empty())
                    continue;

                sigma[s] = 1.0;
                d[s] = 0;
                Q[q_tail++] = s; // enqueue

                // 2. BFS to find shortest paths
                while (q_head != q_tail) { // !queue.empty
                    int v = Q[q_head++];   // pop from queue
                    S[s_top++] = v;        // push to stack

                    for (int w : adj[v]) {
                        // w found for the first time?
                        if (d[w] < 0) {
                            Q[q_tail++] = w;
                            d[w] = d[v] + 1;
                        }
                        // shortest path to w via v?
                        if (d[w] == d[v] + 1) {
                            sigma[w] += sigma[v];
                            P[w].push_back(v);
                        }
                    }
                }

                // 3. Dependency accumulation
                while (s_top != 0) {    // !stack.empty
                    int w = S[--s_top]; // pop from stack
                    for (int v : P[w]) {
                        delta[v] += (sigma[v] / sigma[w]) * (1.0 + delta[w]);
                    }
                    if (w != s) {
                        local_centrality[w] += delta[w]; // Update local buffer
                    }

                    // Reset intermediate structures here to reduce complexity
                    P[w].clear();
                    sigma[w] = 0.0;
                    d[w] = -1;
                    delta[w] = 0.0;
                }
                // reset stack and queue ptrs explicitly
                q_head = 0;
                q_tail = 0;
                s_top = 0;

            } // End of local work

            // Reduce local_centrality to global centrality using mutex
            {
                lock_guard<mutex> lock(centrality_mutex);
                for (int i = 0; i < vertices; ++i) {
                    centrality[i] += local_centrality[i];
                }
            }
        };

        // Spawn threads
        cout << "Running with " << num_threads << " threads..." << endl;
        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker, i);
        }

        // Join threads
        for (auto& t : threads) {
            t.join();
        }

        // 4. Undirected Graph Correction
        // In an undirected graph, the algorithm counts every shortest path twice
        // (once from s to t, and once from t to s). We must halve the final scores.
        for (int i = 0; i < vertices; i++) {
            centrality[i] /= 2.0;
        }
    }

    // A quick helper function to print the results
    void printCentrality() const {
        cout << "Betweenness Centrality Scores:" << endl;
        vector<pair<double, int>> sorted_centrality;
        sorted_centrality.reserve(vertices);
        for (int i = 0; i < vertices; i++) {
            sorted_centrality.emplace_back(centrality[i], i);
        }

        sort(sorted_centrality.rbegin(), sorted_centrality.rend());

        int limit = min(20, vertices);
        for (int i = 0; i < limit; i++) {
            cout << "Vertex " << i << ": " << sorted_centrality[i].first << endl;
        }
    }
};

Centrality* loadGraph(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file " << filename << endl;
        exit(1);
    }

    vector<pair<int, int>> edges;
    int u, v;
    int maxV = 0;
    string line;

    while (getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        stringstream ss(line);
        if (ss >> u >> v) {
            edges.emplace_back(u, v);
            maxV = max({u, v, maxV});
        }
    }
    int numVertices = maxV + 1;
    cout << "Graph info: " << numVertices << " vertices, " << edges.size() << " edges\n";

    Centrality* graph = new Centrality(numVertices);

    for (const auto& edge : edges)
        graph->addEdge(edge.first, edge.second);

    return graph;
}

void display_memory_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    // cout << "Peak memory usage: " << usage.ru_maxrss / (1024 * 1024) << "MB" << endl; // for macos, unit is in bytes
    cout << "Peak memory usage: " << usage.ru_maxrss / 1024 << "MB" << endl; // for linux, unit is in kb, uncomment this line
}

int main(int argc, char* argv[]) {
    string filename = "./datasets/email-Enron.txt";
    if (argc > 1)
        filename = argv[1];

    Centrality* graph = loadGraph(filename);

    auto start = chrono::high_resolution_clock::now();
    graph->calculateBrandes();
    auto end = chrono::high_resolution_clock::now();

    chrono::duration<double> diff = end - start;
    cout << "Calculation finished in " << diff.count() << " seconds." << endl;

    graph->printCentrality();
    display_memory_usage();

    delete graph;

    return 0;
}
