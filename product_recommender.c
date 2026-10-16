/*
 * Product Recommendation by Similarity (DAA mini project)
 *
 * Algorithms used: Merge Sort, Quick Sort, Binary Search,
 * graph (adjacency list), BFS and DFS.
 *
 * Compile:  gcc product_recommender.c -o recommender -lm
 * Run:      recommender <threshold> <command> [arguments]
 * gui.py runs this program every time a button is clicked.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#define MAXN 100        /* maximum number of products */
#define INF -1          /* "infinity" for distances and for the merge sentinel */
#define NIL -1          /* "no parent" */
#define NF 8            /* number of features */

/* each feature is one bit, so a product's features fit in one number */
enum { WL = 1, WP = 2, FC = 4, BT = 8, G5 = 16, TS = 32, NC = 64, EC = 128 };
const char *FEAT[NF] = {"Wireless", "Waterproof", "FastCharge", "Bluetooth",
                        "5G", "Touchscreen", "NoiseCancel", "Eco"};

typedef struct {
    int id;
    char name[40], category[20], brand[20];
    double price, rating;
    int features;
} Product;

Product *P;             /* all the products */
int n = 0;              /* number of products */
int threshold = 45;     /* two products are linked if similarity >= threshold */

int sort_key = 1;       /* 0 id, 1 price, 2 rating, 3 name, 4 similarity */
int descending = 0;
int ref = 0;            /* product used when sorting by similarity */
int use_random = 1;     /* 1 = random pivot in quick sort */
long long compares = 0; /* counts comparisons made by the sorts */

/* small helper functions */

/* compares two texts ignoring upper/lower case; len < 0 means the whole text */
int compare_text(const char *a, const char *b, int len) {
    for (int i = 0; len < 0 || i < len; i++) {
        int x = tolower((unsigned char)a[i]);
        int y = tolower((unsigned char)b[i]);
        if (x != y) return x - y;
        if (x == 0) return 0;
    }
    return 0;
}

/* counts how many bits are 1 */
int count_bits(int x) {
    int c = 0;
    while (x) {
        c += x & 1;
        x >>= 1;
    }
    return c;
}

/* products and the products.csv file */

void add_product(const char *name, const char *cat, const char *brand,
                 double price, double rating, int feat) {
    if (n >= MAXN) return;
    Product *p = &P[n];
    p->id = n + 1;
    strcpy(p->name, name);
    strcpy(p->category, cat);
    strcpy(p->brand, brand);
    p->price = price;
    p->rating = rating;
    p->features = feat;
    n++;
}

void load_samples(void) {
    add_product("Galaxy_M34", "Phone", "Samsung", 18999, 4.2, G5 | FC | TS);
    add_product("Redmi_Note13", "Phone", "Xiaomi", 16999, 4.1, G5 | FC | TS);
    add_product("iPhone_13", "Phone", "Apple", 52999, 4.6, G5 | TS | WP);
    add_product("Realme_12", "Phone", "Realme", 17499, 4.0, G5 | FC | TS);
    add_product("OnePlus_Nord", "Phone", "OnePlus", 24999, 4.3, G5 | FC | TS | WP);
    add_product("boAt_Rockerz", "Headphone", "boAt", 1799, 4.1, WL | BT | FC);
    add_product("Sony_WH1000", "Headphone", "Sony", 24990, 4.7, WL | BT | NC | FC);
    add_product("JBL_Tune", "Headphone", "JBL", 4999, 4.3, WL | BT | FC);
    add_product("Samsung_Buds", "Headphone", "Samsung", 8999, 4.2, WL | BT | NC | WP);
    add_product("Apple_AirPods", "Headphone", "Apple", 24900, 4.6, WL | BT | NC | WP);
    add_product("HP_Pavilion", "Laptop", "HP", 55999, 4.2, TS | FC);
    add_product("Dell_Inspiron", "Laptop", "Dell", 48999, 4.1, TS);
    add_product("Lenovo_IdeaPad", "Laptop", "Lenovo", 42999, 4.0, TS | FC);
    add_product("MacBook_Air", "Laptop", "Apple", 99999, 4.8, FC | EC);
    add_product("Asus_Vivobook", "Laptop", "Asus", 45999, 4.2, TS | FC);
    add_product("Noise_ColorFit", "Watch", "Noise", 2499, 4.0, WL | WP | BT | TS);
    add_product("boAt_Wave", "Watch", "boAt", 1999, 3.9, WL | WP | BT | TS);
    add_product("Apple_Watch_SE", "Watch", "Apple", 29900, 4.6, WP | BT | TS | FC);
    add_product("Samsung_Watch4", "Watch", "Samsung", 17999, 4.3, WP | BT | TS);
    add_product("Fitbit_Charge", "Watch", "Fitbit", 14999, 4.2, WP | BT);
}

void save_file(void) {
    FILE *fp = fopen("products.csv", "w");
    if (!fp) return;
    for (int i = 0; i < n; i++)
        fprintf(fp, "%s,%s,%s,%g,%.1f,%d\n", P[i].name, P[i].category, P[i].brand,
                P[i].price, P[i].rating, P[i].features);
    fclose(fp);
}

/* reads products.csv, or creates it with the sample products if it is missing */
void load_file(void) {
    FILE *fp = fopen("products.csv", "r");
    if (!fp) {
        load_samples();
        save_file();
        return;
    }
    char line[200], name[40], cat[20], brand[20];
    double price, rating;
    int feat;
    while (fgets(line, sizeof line, fp))
        if (sscanf(line, "%39[^,],%19[^,],%19[^,],%lf,%lf,%d",
                   name, cat, brand, &price, &rating, &feat) == 6)
            add_product(name, cat, brand, price, rating, feat);
    fclose(fp);
}

/* prints one product as a tab separated line (gui.py reads this) */
void print_row(int i) {
    printf("%d\t%s\t%s\t%s\t%.0f\t%.1f\t", P[i].id, P[i].name, P[i].category,
           P[i].brand, P[i].price, P[i].rating);
    int any = 0;
    for (int k = 0; k < NF; k++)
        if (P[i].features & (1 << k)) {
            printf("%s%s", any ? ", " : "", FEAT[k]);
            any = 1;
        }
    if (!any) printf("-");
    printf("\n");
}

/* similarity of two products, a score from 0 to 100 */
double similarity(int a, int b) {
    double s = 0;
    if (!strcmp(P[a].category, P[b].category)) s += 40;      /* same category */
    if (!strcmp(P[a].brand, P[b].brand)) s += 15;            /* same brand */

    double high = P[a].price > P[b].price ? P[a].price : P[b].price;
    double low = P[a].price < P[b].price ? P[a].price : P[b].price;
    s += 20.0 * low / high;                                  /* close prices */

    s += 10.0 * (1.0 - fabs(P[a].rating - P[b].rating) / 5.0);   /* close ratings */

    int all = count_bits(P[a].features | P[b].features);
    int common = count_bits(P[a].features & P[b].features);
    if (all) s += 15.0 * common / all;                       /* shared features */
    return s;
}

/* sorting: the sorts work on an array of product numbers, not on the products */

/* returns 1 if product a should come before product b */
int before(int a, int b) {
    double diff;
    compares++;
    switch (sort_key) {
        case 0: diff = P[a].id - P[b].id; break;
        case 1: diff = P[a].price - P[b].price; break;
        case 2: diff = P[a].rating - P[b].rating; break;
        case 3: diff = compare_text(P[a].name, P[b].name, -1); break;
        default: diff = similarity(ref, b) - similarity(ref, a); break;
    }
    return descending ? diff > 0 : diff < 0;
}

/* a <= b, where INF (the sentinel) counts as bigger than everything */
int leq(int a, int b) {
    if (b == INF) return 1;
    if (a == INF) return 0;
    return !before(b, a);
}

/* Merge Sort */

void merge(int *A, int p, int q, int r) {
    int n1 = q - p + 1;
    int n2 = r - q;
    int *L = malloc((n1 + 1) * sizeof(int));
    int *R = malloc((n2 + 1) * sizeof(int));
    for (int i = 0; i < n1; i++) L[i] = A[p + i];
    for (int j = 0; j < n2; j++) R[j] = A[q + 1 + j];
    L[n1] = INF;                 /* sentinels, so we never run off the end */
    R[n2] = INF;
    int i = 0, j = 0;
    for (int k = p; k <= r; k++) {
        if (leq(L[i], R[j])) A[k] = L[i++];
        else A[k] = R[j++];
    }
    free(L);
    free(R);
}

void merge_sort(int *A, int p, int r) {
    if (p < r) {
        int q = (p + r) / 2;
        merge_sort(A, p, q);
        merge_sort(A, q + 1, r);
        merge(A, p, q, r);
    }
}

/* Quick Sort */

void swap(int *A, int i, int j) {
    int t = A[i];
    A[i] = A[j];
    A[j] = t;
}

int partition(int *A, int p, int r) {
    int x = A[r];                          /* pivot = last element */
    int i = p - 1;
    for (int j = p; j <= r - 1; j++) {
        if (!before(x, A[j])) {            /* A[j] <= x */
            i = i + 1;
            swap(A, i, j);
        }
    }
    swap(A, i + 1, r);
    return i + 1;
}

/* picks a random pivot first, then partitions as usual */
int random_partition(int *A, int p, int r) {
    int k = p + rand() % (r - p + 1);
    swap(A, r, k);
    return partition(A, p, r);
}

void quick_sort(int *A, int p, int r) {
    if (p < r) {
        int q;
        if (use_random) q = random_partition(A, p, r);
        else q = partition(A, p, r);
        quick_sort(A, p, q - 1);
        quick_sort(A, q + 1, r);
    }
}

/* sorts A[0..cnt-1] with merge sort (use_merge = 1) or quick sort (0) */
void run_sort(int *A, int cnt, int use_merge) {
    compares = 0;
    if (cnt < 2) return;
    if (use_merge) merge_sort(A, 0, cnt - 1);
    else quick_sort(A, 0, cnt - 1);
}

/* gives back all products sorted with merge sort (needed before binary search) */
int *sorted_list(int key) {
    int *A = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) A[i] = i;
    sort_key = key;
    descending = 0;
    run_sort(A, n, 1);
    return A;
}

/* Binary Search: both functions return the first position whose value is >= key */

int search_name(int *A, int cnt, const char *key, int *steps) {
    int low = 0, high = cnt;
    while (low < high) {
        int mid = (low + high) / 2;
        (*steps)++;
        if (compare_text(P[A[mid]].name, key, -1) < 0) low = mid + 1;
        else high = mid;
    }
    return low;
}

int search_price(int *A, int cnt, double key, int *steps) {
    int low = 0, high = cnt;
    while (low < high) {
        int mid = (low + high) / 2;
        (*steps)++;
        if (P[A[mid]].price < key) low = mid + 1;
        else high = mid;
    }
    return low;
}

/* graph as adjacency lists */

typedef struct Edge {
    int to, w;               /* neighbour and similarity score */
    struct Edge *next;
} Edge;

Edge *Adj[MAXN];

void build_graph(void) {
    for (int i = n - 1; i >= 0; i--)
        for (int j = n - 1; j >= 0; j--) {
            int s = (int)(similarity(i, j) + 0.5);
            if (i != j && s >= threshold) {
                Edge *e = malloc(sizeof(Edge));
                e->to = j;
                e->w = s;
                e->next = Adj[i];
                Adj[i] = e;
            }
        }
}

enum { WHITE, GRAY, BLACK };
int color[MAXN];             /* colour of each vertex */
int d[MAXN];                 /* BFS: distance from start.  DFS: discovery time */
int f[MAXN];                 /* DFS finish time */
int pi[MAXN];                /* parent */

/* BFS(G, s): returns how many vertices were reached (including s) */
int bfs(int s) {
    int Q[MAXN];
    int head = 0, tail = 0;
    for (int u = 0; u < n; u++) {
        if (u != s) {
            color[u] = WHITE;
            d[u] = INF;
            pi[u] = NIL;
        }
    }
    color[s] = GRAY;
    d[s] = 0;
    pi[s] = NIL;
    Q[tail++] = s;                       /* enqueue */
    while (head < tail) {                /* while Q is not empty */
        int u = Q[head++];               /* dequeue */
        for (Edge *e = Adj[u]; e; e = e->next) {
            int v = e->to;
            if (color[v] == WHITE) {
                color[v] = GRAY;
                d[v] = d[u] + 1;
                pi[v] = u;
                Q[tail++] = v;
            }
        }
        color[u] = BLACK;
    }
    return tail;
}

/* DFS */
int timer;
int order[MAXN];             /* vertices in the order they were discovered */
int ocount;
int depth[MAXN];             /* how deep each vertex is in the DFS tree */

void dfs_visit(int u) {
    color[u] = GRAY;
    timer = timer + 1;
    d[u] = timer;
    order[ocount++] = u;
    for (Edge *e = Adj[u]; e; e = e->next) {
        int v = e->to;
        if (color[v] == WHITE) {
            pi[v] = u;
            depth[v] = depth[u] + 1;
            dfs_visit(v);
        }
    }
    color[u] = BLACK;
    timer = timer + 1;
    f[u] = timer;
}

/* DFS(G): we only start from the chosen product, not from every vertex */
void dfs(int start) {
    for (int u = 0; u < n; u++) {
        color[u] = WHITE;
        pi[u] = NIL;
        depth[u] = 0;
    }
    timer = 0;
    ocount = 0;
    dfs_visit(start);
}

/* commands (each one is called from main) */

int valid_id(int id) {
    if (id >= 1 && id <= n) return 1;
    printf("#Invalid product ID\n");
    return 0;
}

void list_all(void) {
    for (int i = 0; i < n; i++) print_row(i);
    printf("#%d products loaded\n", n);
}

/* key: 1 price, 2 rating, 3 name, 4 id.  alg: 1 merge sort, 2 quick sort.  order: 1 up, 2 down */
void sort_products(int key, int alg, int order) {
    sort_key = (key == 4) ? 0 : (key >= 1 && key <= 3 ? key : 1);
    descending = (order == 2);
    use_random = 1;
    int *A = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) A[i] = i;
    run_sort(A, n, alg != 2);
    for (int i = 0; i < n; i++) print_row(A[i]);
    printf("#%s: %lld comparisons for n = %d  (time O(n log n))\n",
           alg == 2 ? "Randomized Quick Sort" : "Merge Sort", compares, n);
    free(A);
}

/* finds every product whose name starts with the given text */
void find_by_name(const char *text) {
    int len = strlen(text), steps = 0, found = 0;
    int *A = sorted_list(3);
    int pos = search_name(A, n, text, &steps);
    for (int i = pos; i < n && compare_text(P[A[i]].name, text, len) == 0; i++) {
        print_row(A[i]);
        found++;
    }
    printf("#%d found. Binary search took %d steps on %d products (list sorted with Merge Sort first)\n",
           found, steps, n);
    free(A);
}

/* finds every product with min <= price <= max */
void find_by_price(double min, double max) {
    int steps = 0, found = 0;
    int *A = sorted_list(1);
    int pos = search_price(A, n, min, &steps);
    for (int i = pos; i < n && P[A[i]].price <= max; i++) {
        print_row(A[i]);
        found++;
    }
    printf("#%d found. Binary search took %d steps to locate the start of the range\n", found, steps);
    free(A);
}

void show_network(void) {
    int total = 0;
    printf("Similarity network as adjacency list. Edge exists if similarity >= %d\n", threshold);
    printf("Format: product -> neighbour(similarity)\n\n");
    for (int i = 0; i < n; i++) {
        printf("%-3d %-15s -> ", P[i].id, P[i].name);
        int deg = 0;
        for (Edge *e = Adj[i]; e; e = e->next) {
            printf("%d(%d) ", P[e->to].id, e->w);
            deg++;
        }
        if (!deg) printf("(isolated)");
        printf("\n");
        total += deg;
    }
    printf("\nV = %d, E = %d, memory O(V+E)\n", n, total / 2);
}

/* gives the dots and lines for the picture; id = 0 means no colouring */
void graph_data(int id) {
    if (id >= 1 && id <= n) bfs(id - 1);
    else for (int i = 0; i < n; i++) d[i] = INF;
    for (int i = 0; i < n; i++) printf("N\t%s\t%d\n", P[i].name, d[i]);
    for (int i = 0; i < n; i++)
        for (Edge *e = Adj[i]; e; e = e->next)
            if (i < e->to) printf("E\t%d\t%d\t%d\n", i, e->to, e->w);
}

/* recommendations for the screen: one line per product, closest levels first */
void similar(int id, double budget) {
    if (!valid_id(id)) return;
    int s = id - 1, maxd = 0;
    bfs(s);
    for (int i = 0; i < n; i++)
        if (d[i] > maxd) maxd = d[i];
    ref = s;
    sort_key = 4;
    descending = 0;
    for (int lvl = 1; lvl <= maxd; lvl++) {
        int group[MAXN], cnt = 0;
        for (int i = 0; i < n; i++)
            if (d[i] == lvl) group[cnt++] = i;
        run_sort(group, cnt, 1);                 /* most similar first */
        for (int k = 0; k < cnt; k++) {
            int v = group[k];
            if (budget > 0 && P[v].price > budget) continue;
            printf("S\t%d\t%.0f\t%s\t", lvl, similarity(s, v), P[pi[v]].name);
            print_row(v);
        }
    }
}

/* the "keep exploring" path for the screen */
void trail(int id) {
    if (!valid_id(id)) return;
    dfs(id - 1);
    for (int k = 1; k < ocount; k++) {
        printf("T\t%d\t", depth[order[k]]);
        print_row(order[k]);
    }
}

/* written report of the BFS recommendations (developer mode) */
void recommend_report(int id, double budget, const char *pref) {
    if (!valid_id(id)) return;
    int s = id - 1, maxd = 0;
    int reached = bfs(s);
    for (int i = 0; i < n; i++)
        if (d[i] > maxd) maxd = d[i];
    printf("Recommendations for: %s  (%s, %s, Rs %.0f, rating %.1f)\n", P[s].name,
           P[s].brand, P[s].category, P[s].price, P[s].rating);
    printf("Found using BFS. Products are grouped by distance in the similarity network\n");
    printf("and ranked inside each group by similarity (Merge Sort).\n");
    ref = s;
    sort_key = 4;
    descending = 0;
    for (int lvl = 1; lvl <= maxd; lvl++) {
        int group[MAXN], cnt = 0;
        for (int i = 0; i < n; i++)
            if (d[i] == lvl) group[cnt++] = i;
        run_sort(group, cnt, 1);
        printf("\nLevel %d  (%s)\n", lvl,
               lvl == 1 ? "directly similar" : "similar to a product of the level above");
        for (int k = 0; k < cnt; k++) {
            int v = group[k];
            if (budget > 0 && P[v].price > budget) continue;
            int fav = pref[0] && strcmp(pref, "-") && !compare_text(P[v].brand, pref, -1);
            printf("   %-15s %-10s Rs %-7.0f rating %.1f   similarity %3.0f%%   via %s%s\n",
                   P[v].name, P[v].category, P[v].price, P[v].rating, similarity(s, v),
                   P[pi[v]].name, fav ? "   [preferred brand]" : "");
        }
    }
    if (reached == 1) printf("\nNo similar products at threshold %d. Lower the threshold.\n", threshold);
    printf("\nBFS reached %d of %d other products. Time O(V+E).\n", reached - 1, n - 1);
}

/* written report of the DFS tree (developer mode) */
void dfs_report(int id) {
    if (!valid_id(id)) return;
    dfs(id - 1);
    printf("DFS tree from %s. Indent shows depth. d = discovery time, f = finish time\n\n", P[id - 1].name);
    for (int k = 0; k < ocount; k++) {
        int u = order[k];
        printf("   ");
        for (int i = 0; i < depth[u]; i++) printf("    ");
        printf("%-15s d=%-2d f=%-2d\n", P[u].name, d[u], f[u]);
    }
    printf("\nDFS visited %d products. Time O(V+E).\n", ocount);
    printf("DFS follows one chain deeply, while BFS lists the closest matches first,\n");
    printf("so BFS is the better fit for recommendations.\n");
}

double elapsed_ms(clock_t start) {
    return 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
}

/* compares the two sorts on big random data and on already sorted data */
void speed_test(void) {
    Product *saved = P;
    int saved_n = n;
    P = malloc(100000 * sizeof(Product));    /* temporary big list, only prices matter */
    sort_key = 1;
    descending = 0;
    int sizes[] = {1000, 10000, 50000, 100000};

    printf("Sorting by price on RANDOM data\n\n");
    printf("%-9s %-26s %-26s\n", "n", "Merge Sort (ms, compares)", "Rand. Quick (ms, compares)");
    for (int s = 0; s < 4; s++) {
        int m = sizes[s];
        int *a = malloc(m * sizeof(int));
        int *b = malloc(m * sizeof(int));
        for (int i = 0; i < m; i++) {
            P[i].price = 100 + rand() % 100000;
            a[i] = b[i] = i;
        }
        clock_t t = clock();
        run_sort(a, m, 1);
        double merge_time = elapsed_ms(t);
        long long merge_compares = compares;

        use_random = 1;
        t = clock();
        run_sort(b, m, 0);
        double quick_time = elapsed_ms(t);
        printf("%-9d %9.2f %14lld     %9.2f %14lld\n", m, merge_time, merge_compares,
               quick_time, compares);
        free(a);
        free(b);
    }

    int m = 10000;
    int *a = malloc(m * sizeof(int));
    printf("\nALREADY SORTED data, n = %d (worst case of plain Quick Sort)\n\n", m);
    for (int i = 0; i < m; i++) P[i].price = i;
    for (int v = 0; v < 3; v++) {
        for (int i = 0; i < m; i++) a[i] = i;
        use_random = (v == 2);
        clock_t t = clock();
        run_sort(a, m, v == 0);
        double time_taken = elapsed_ms(t);
        printf("%-26s %9.2f ms %14lld compares\n",
               v == 0 ? "Merge Sort" : v == 1 ? "Quick Sort (last pivot)" : "Randomized Quick Sort",
               time_taken, compares);
    }
    free(a);
    free(P);
    P = saved;
    n = saved_n;
    use_random = 1;
}

void complexity_table(void) {
    printf("%-20s %-10s %-10s %-12s %-10s\n", "Algorithm", "Best", "Average", "Worst", "Space");
    printf("%-20s %-10s %-10s %-12s %-10s\n", "Merge Sort", "n log n", "n log n", "n log n", "O(n)");
    printf("%-20s %-10s %-10s %-12s %-10s\n", "Quick Sort", "n log n", "n log n", "n^2", "O(log n)");
    printf("%-20s %-10s %-10s %-12s %-10s\n", "Randomized Quick", "n log n", "n log n", "n^2 (rare)", "O(log n)");
    printf("%-20s %-10s %-10s %-12s %-10s\n", "Binary Search", "O(1)", "O(log n)", "O(log n)", "O(1)");
    printf("%-20s %-10s %-10s %-12s %-10s\n", "BFS / DFS (list)", "O(V+E)", "O(V+E)", "O(V+E)", "O(V)");
    printf("\nBuilding the graph needs O(V^2) similarity checks. Storage is O(V+E).\n");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("usage: recommender <threshold> <command> [arguments]\n");
        return 1;
    }
#define ARG(i) ((i) < argc ? argv[i] : "")
    threshold = atoi(argv[1]);
    const char *cmd = argv[2];

    srand((unsigned)time(NULL));
    P = malloc(MAXN * sizeof(Product));
    load_file();
    build_graph();

    if (!strcmp(cmd, "list")) list_all();
    else if (!strcmp(cmd, "sort")) sort_products(atoi(ARG(3)), atoi(ARG(4)), atoi(ARG(5)));
    else if (!strcmp(cmd, "sname")) find_by_name(ARG(3));
    else if (!strcmp(cmd, "sprice")) find_by_price(atof(ARG(3)), atof(ARG(4)));
    else if (!strcmp(cmd, "net")) show_network();
    else if (!strcmp(cmd, "graph")) graph_data(atoi(ARG(3)));
    else if (!strcmp(cmd, "rec")) recommend_report(atoi(ARG(3)), atof(ARG(4)), ARG(5));
    else if (!strcmp(cmd, "dfs")) dfs_report(atoi(ARG(3)));
    else if (!strcmp(cmd, "similar")) similar(atoi(ARG(3)), atof(ARG(4)));
    else if (!strcmp(cmd, "trail")) trail(atoi(ARG(3)));
    else if (!strcmp(cmd, "bench")) speed_test();
    else if (!strcmp(cmd, "complexity")) complexity_table();
    else printf("#Unknown command\n");
    return 0;
}
