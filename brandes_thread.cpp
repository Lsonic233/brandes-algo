#include <iostream>
#include <vector>
#include <stack>
#include <queue>
#include <omp.h>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

class Centrality {
private:
    int vertices;
    vector<vector<int>> adj;
    vector<double> centrality;

public:
    // Constructor to initialize the graph's size
    Centrality(int v) : vertices(v), adj(v), centrality(v, 0.0) {}

    // Adds an edge for an UNDIRECTED graph
    void addEdge(int u, int v) {
        adj[u].push_back(v);
        adj[v].push_back(u); 
    }

    void calculateBrandes() {
        // Parallel region
        #pragma omp parallel
        {
            // Thread-private data structures initialized once per thread
            vector<vector<int>> P(vertices);
            vector<double> sigma(vertices);
            vector<int> d(vertices);
            vector<double> delta(vertices);
            queue<int> Q;
            stack<int> S;
            
            // Thread-local accumulation buffer to avoid race conditions
            vector<double> local_centrality(vertices, 0.0);

            #pragma omp for schedule(dynamic)
            for (int s = 0; s < vertices; s++) {
                // 1. Reset the intermediate data structures for the new source vertex
                for (int i = 0; i < vertices; i++) {
                    P[i].clear();
                    sigma[i] = 0.0;
                    d[i] = -1;
                    delta[i] = 0.0;
                }
                
                sigma[s] = 1.0;
                d[s] = 0;
                Q.push(s);

                // 2. BFS to find shortest paths
                while (!Q.empty()) {
                    int v = Q.front();
                    Q.pop();
                    S.push(v);

                    for (int w : adj[v]) {
                        // w found for the first time?
                        if (d[w] < 0) {
                            Q.push(w);
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
                while (!S.empty()) {
                    int w = S.top();
                    S.pop();
                    for (int v : P[w]) {
                        delta[v] += (sigma[v] / sigma[w]) * (1.0 + delta[w]);
                    }
                    if (w != s) {
                        local_centrality[w] += delta[w]; // Update local buffer
                    }
                }
            } // End of for loop

            // Reduce local_centrality to global centrality
            #pragma omp critical
            {
                for(int i = 0; i < vertices; ++i) {
                    centrality[i] += local_centrality[i];
                }
            }
        } // End parallel region

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
        
        vector<double> sorted_centrality {centrality.begin(), centrality.end()};
        sort(sorted_centrality.rbegin(), sorted_centrality.rend());
        int limit = min(20, vertices);
        // bug: vertex index printed is wrong due to sorting, need to handle
        for (int i = 0; i < limit; i++) {
            cout << "Vertex " << i << ": " << sorted_centrality[i] << endl;
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
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        if (ss >> u >> v) {
            edges.emplace_back(u, v);
            maxV = max({u, v, maxV});
        }
        
    }
    int numVertices = maxV + 1;
    cout << "Graph info: " << numVertices << " vertices, " << edges.size() << " edges\n";

    Centrality* graph = new Centrality(numVertices);

    for (const auto& edge: edges)
        graph->addEdge(edge.first, edge.second);

    return graph;
}

int main(int argc, char* argv[]) {
    string filename = "./datasets/email-Enron.txt";
    if (argc > 1)
        filename = argv[1];

    Centrality* graph = loadGraph(filename);
    
    double start = omp_get_wtime();    
    graph->calculateBrandes();
    double end = omp_get_wtime();
    cout << "Calculation finished in " << (end - start) << " seconds." << endl;

    graph->printCentrality();

    delete graph;

    return 0;
}