/*
    EXPENSE SPLITTER  (Splitwise-style backend)
    --------------------------------------------
    C++17 | OOP | STL | File Handling | Hash Maps | Greedy Debt Simplification

    Classes:
      User             -> user profile
      Expense          -> single expense with split details
      Group            -> collection of users + their expenses + balances
      BalanceManager   -> applies expenses to group balances
      SettlementManager-> minimum-transaction debt simplification (greedy)
      Storage          -> file persistence (load/save all data)
      ExpenseSplitter  -> facade tying everything together + CLI

    Persistence files (pipe delimited, plain text):
      users.dat     : id|name|email
      groups.dat    : id|name|m1,m2,m3...
      expenses.dat  : id|groupId|description|amount|paidBy|splitType|uid:amt,uid:amt...
*/

#include <bits/stdc++.h>
using namespace std;

// ------------------------------------------------------------------
// USER
// ------------------------------------------------------------------
class User {
public:
    int id;
    string name, email;
    User() : id(0) {}
    User(int id_, string n, string e) : id(id_), name(move(n)), email(move(e)) {}
};

// ------------------------------------------------------------------
// EXPENSE
// ------------------------------------------------------------------
enum class SplitType { EQUAL, EXACT, PERCENT };

inline string splitTypeToStr(SplitType t) {
    switch (t) {
        case SplitType::EQUAL: return "EQUAL";
        case SplitType::EXACT: return "EXACT";
        case SplitType::PERCENT: return "PERCENT";
    }
    return "EQUAL";
}
inline SplitType strToSplitType(const string& s) {
    if (s == "EXACT") return SplitType::EXACT;
    if (s == "PERCENT") return SplitType::PERCENT;
    return SplitType::EQUAL;
}

class Expense {
public:
    int id;
    int groupId;
    string description;
    double amount;
    int paidBy;
    SplitType type;
    unordered_map<int, double> shares; // userId -> amount owed for this expense

    Expense() : id(0), groupId(0), amount(0), paidBy(0), type(SplitType::EQUAL) {}
    Expense(int id_, int gId, string desc, double amt, int payer, SplitType t,
            unordered_map<int, double> sh)
        : id(id_), groupId(gId), description(move(desc)), amount(amt),
          paidBy(payer), type(t), shares(move(sh)) {}

    // Builds per-user shares given a split type and raw input values.
    static unordered_map<int, double> computeShares(
        SplitType type, double amount, const vector<int>& members,
        const unordered_map<int, double>& input /* exact amt or percent, unused for EQUAL */) {
        unordered_map<int, double> shares;
        if (type == SplitType::EQUAL) {
            double each = amount / static_cast<double>(members.size());
            for (int m : members) shares[m] = each;
        } else if (type == SplitType::EXACT) {
            for (int m : members) shares[m] = input.count(m) ? input.at(m) : 0.0;
        } else { // PERCENT
            for (int m : members) {
                double pct = input.count(m) ? input.at(m) : 0.0;
                shares[m] = amount * pct / 100.0;
            }
        }
        return shares;
    }
};

// ------------------------------------------------------------------
// GROUP
// ------------------------------------------------------------------
class Group {
public:
    int id;
    string name;
    vector<int> memberIds;
    vector<Expense> expenses;
    unordered_map<int, double> balances; // userId -> net balance (+ means owed to them)

    Group() : id(0) {}
    Group(int id_, string n) : id(id_), name(move(n)) {}

    bool hasMember(int uid) const {
        return find(memberIds.begin(), memberIds.end(), uid) != memberIds.end();
    }
    void addMember(int uid) {
        if (!hasMember(uid)) {
            memberIds.push_back(uid);
            balances[uid] += 0.0;
        }
    }
};

// ------------------------------------------------------------------
// BALANCE MANAGER  (single responsibility: keep balances correct)
// ------------------------------------------------------------------
class BalanceManager {
public:
    static void applyExpense(Group& g, const Expense& e) {
        g.balances[e.paidBy] += e.amount;
        for (auto& [uid, share] : e.shares) g.balances[uid] -= share;
    }

    static void recomputeAll(Group& g) {
        g.balances.clear();
        for (int uid : g.memberIds) g.balances[uid] = 0.0;
        for (auto& e : g.expenses) applyExpense(g, e);
    }
};

// ------------------------------------------------------------------
// SETTLEMENT MANAGER (greedy minimum-transaction debt simplification)
// ------------------------------------------------------------------
class SettlementManager {
public:
    struct Transaction { int from, to; double amount; };

    static vector<Transaction> simplify(unordered_map<int, double> balances) {
        const double EPS = 1e-6;
        vector<pair<int, double>> creditors, debtors;
        for (auto& [uid, bal] : balances) {
            if (bal > EPS) creditors.push_back({uid, bal});
            else if (bal < -EPS) debtors.push_back({uid, -bal});
        }
        vector<Transaction> result;
        size_t i = 0, j = 0;
        while (i < debtors.size() && j < creditors.size()) {
            double amt = min(debtors[i].second, creditors[j].second);
            if (amt > EPS) {
                result.push_back({debtors[i].first, creditors[j].first, amt});
                debtors[i].second -= amt;
                creditors[j].second -= amt;
            }
            if (debtors[i].second <= EPS) i++;
            if (creditors[j].second <= EPS) j++;
        }
        return result;
    }
};

// ------------------------------------------------------------------
// STORAGE (file handling / persistence)
// ------------------------------------------------------------------
class Storage {
public:
    static void saveUsers(const unordered_map<int, User>& users) {
        ofstream f("users.dat");
        for (auto& [id, u] : users) f << u.id << "|" << u.name << "|" << u.email << "\n";
    }
    static void saveGroups(const unordered_map<int, Group>& groups) {
        ofstream f("groups.dat");
        for (auto& [id, g] : groups) {
            f << g.id << "|" << g.name << "|";
            for (size_t i = 0; i < g.memberIds.size(); i++)
                f << g.memberIds[i] << (i + 1 < g.memberIds.size() ? "," : "");
            f << "\n";
        }
    }
    static void saveExpenses(const unordered_map<int, Group>& groups) {
        ofstream f("expenses.dat");
        for (auto& [gid, g] : groups) {
            for (auto& e : g.expenses) {
                f << e.id << "|" << e.groupId << "|" << e.description << "|" << e.amount
                  << "|" << e.paidBy << "|" << splitTypeToStr(e.type) << "|";
                size_t k = 0;
                for (auto& [uid, sh] : e.shares) {
                    f << uid << ":" << sh << (++k < e.shares.size() ? "," : "");
                }
                f << "\n";
            }
        }
    }
    static void saveAll(const unordered_map<int, User>& users,
                         const unordered_map<int, Group>& groups) {
        saveUsers(users);
        saveGroups(groups);
        saveExpenses(groups);
    }

    static vector<string> splitStr(const string& s, char delim) {
        vector<string> out;
        stringstream ss(s);
        string item;
        while (getline(ss, item, delim)) out.push_back(item);
        return out;
    }

    static void loadUsers(unordered_map<int, User>& users, int& nextUserId) {
        ifstream f("users.dat");
        string line;
        while (getline(f, line)) {
            if (line.empty()) continue;
            auto parts = splitStr(line, '|');
            if (parts.size() < 3) continue;
            int id = stoi(parts[0]);
            users[id] = User(id, parts[1], parts[2]);
            nextUserId = max(nextUserId, id + 1);
        }
    }
    static void loadGroups(unordered_map<int, Group>& groups, int& nextGroupId) {
        ifstream f("groups.dat");
        string line;
        while (getline(f, line)) {
            if (line.empty()) continue;
            auto parts = splitStr(line, '|');
            if (parts.size() < 2) continue;
            int id = stoi(parts[0]);
            Group g(id, parts[1]);
            if (parts.size() >= 3 && !parts[2].empty()) {
                for (auto& m : splitStr(parts[2], ',')) g.addMember(stoi(m));
            }
            groups[id] = g;
            nextGroupId = max(nextGroupId, id + 1);
        }
    }
    static void loadExpenses(unordered_map<int, Group>& groups, int& nextExpenseId) {
        ifstream f("expenses.dat");
        string line;
        while (getline(f, line)) {
            if (line.empty()) continue;
            auto parts = splitStr(line, '|');
            if (parts.size() < 6) continue;
            int id = stoi(parts[0]);
            int gid = stoi(parts[1]);
            string desc = parts[2];
            double amt = stod(parts[3]);
            int paidBy = stoi(parts[4]);
            SplitType type = strToSplitType(parts[5]);
            unordered_map<int, double> shares;
            if (parts.size() >= 7) {
                for (auto& kv : splitStr(parts[6], ',')) {
                    if (kv.empty()) continue;
                    auto pr = splitStr(kv, ':');
                    if (pr.size() == 2) shares[stoi(pr[0])] = stod(pr[1]);
                }
            }
            Expense e(id, gid, desc, amt, paidBy, type, shares);
            if (groups.count(gid)) {
                groups[gid].expenses.push_back(e);
                nextExpenseId = max(nextExpenseId, id + 1);
            }
        }
        for (auto& [gid, g] : groups) BalanceManager::recomputeAll(g);
    }
};

// ------------------------------------------------------------------
// EXPENSE SPLITTER  (facade / application layer + CLI)
// ------------------------------------------------------------------
class ExpenseSplitter {
    unordered_map<int, User> users;
    unordered_map<int, Group> groups;
    int nextUserId = 1, nextGroupId = 1, nextExpenseId = 1;

public:
    ExpenseSplitter() {
        Storage::loadUsers(users, nextUserId);
        Storage::loadGroups(groups, nextGroupId);
        Storage::loadExpenses(groups, nextExpenseId);
    }

    void save() { Storage::saveAll(users, groups); }

    // ---------- User management ----------
    int registerUser(const string& name, const string& email) {
        int id = nextUserId++;
        users[id] = User(id, name, email);
        return id;
    }
    User* findUser(int id) { return users.count(id) ? &users[id] : nullptr; }
    void listUsers() const {
        cout << "\n-- Users --\n";
        for (auto& [id, u] : users)
            cout << "  [" << id << "] " << u.name << " <" << u.email << ">\n";
    }

    // ---------- Group management ----------
    int createGroup(const string& name) {
        int id = nextGroupId++;
        groups[id] = Group(id, name);
        return id;
    }
    Group* findGroup(int id) { return groups.count(id) ? &groups[id] : nullptr; }
    void listGroups() const {
        cout << "\n-- Groups --\n";
        for (auto& [id, g] : groups)
            cout << "  [" << id << "] " << g.name << " (" << g.memberIds.size() << " members)\n";
    }
    bool addMemberToGroup(int gid, int uid) {
        if (!groups.count(gid) || !users.count(uid)) return false;
        groups[gid].addMember(uid);
        return true;
    }

    // ---------- Expense management ----------
    bool addExpense(int gid, const string& desc, double amount, int paidBy,
                    SplitType type, const unordered_map<int, double>& input,
                    const vector<int>& participants) {
        if (!groups.count(gid)) return false;
        Group& g = groups[gid];
        for (int p : participants) if (!g.hasMember(p)) return false;

        auto shares = Expense::computeShares(type, amount, participants, input);

        // Validate exact/percent sums (with small tolerance)
        if (type == SplitType::EXACT) {
            double sum = 0; for (auto& [u, s] : shares) sum += s;
            if (fabs(sum - amount) > 0.01) return false;
        } else if (type == SplitType::PERCENT) {
            double sum = 0; for (auto& [u, p] : input) sum += p;
            if (fabs(sum - 100.0) > 0.01) return false;
        }

        Expense e(nextExpenseId++, gid, desc, amount, paidBy, type, shares);
        g.expenses.push_back(e);
        BalanceManager::applyExpense(g, e);
        return true;
    }

    void printExpenseHistory(int gid) {
        if (!groups.count(gid)) { cout << "Group not found.\n"; return; }
        Group& g = groups[gid];
        cout << "\n-- Expense History: " << g.name << " --\n";
        if (g.expenses.empty()) { cout << "  (no expenses yet)\n"; return; }
        for (auto& e : g.expenses) {
            cout << "  #" << e.id << " \"" << e.description << "\"  Rs." << fixed
                 << setprecision(2) << e.amount << "  paid by "
                 << (users.count(e.paidBy) ? users[e.paidBy].name : "?")
                 << "  [" << splitTypeToStr(e.type) << "]\n";
            for (auto& [uid, sh] : e.shares) {
                cout << "      " << (users.count(uid) ? users[uid].name : "?")
                     << " owes Rs." << fixed << setprecision(2) << sh << "\n";
            }
        }
    }

    void printBalances(int gid) {
        if (!groups.count(gid)) { cout << "Group not found.\n"; return; }
        Group& g = groups[gid];
        cout << "\n-- Balances: " << g.name << " --\n";
        for (int uid : g.memberIds) {
            double bal = g.balances.count(uid) ? g.balances[uid] : 0.0;
            string name = users.count(uid) ? users[uid].name : "?";
            if (bal > 1e-6)
                cout << "  " << name << " is owed Rs." << fixed << setprecision(2) << bal << "\n";
            else if (bal < -1e-6)
                cout << "  " << name << " owes Rs." << fixed << setprecision(2) << -bal << "\n";
            else
                cout << "  " << name << " is settled up\n";
        }
    }

    void printSettlement(int gid) {
        if (!groups.count(gid)) { cout << "Group not found.\n"; return; }
        Group& g = groups[gid];
        auto txns = SettlementManager::simplify(g.balances);
        cout << "\n-- Settlement Plan (min transactions): " << g.name << " --\n";
        if (txns.empty()) { cout << "  Everyone is settled up. \n"; return; }
        for (auto& t : txns) {
            string from = users.count(t.from) ? users[t.from].name : "?";
            string to = users.count(t.to) ? users[t.to].name : "?";
            cout << "  " << from << " pays " << to << "  Rs." << fixed
                 << setprecision(2) << t.amount << "\n";
        }
        cout << "  Total transactions: " << txns.size() << "\n";
    }
};

// ------------------------------------------------------------------
// CLI helpers
// ------------------------------------------------------------------
static int readInt(const string& prompt) {
    cout << prompt;
    int v; while (!(cin >> v)) { cin.clear(); cin.ignore(10000, '\n'); cout << prompt; }
    cin.ignore(10000, '\n');
    return v;
}
static double readDouble(const string& prompt) {
    cout << prompt;
    double v; while (!(cin >> v)) { cin.clear(); cin.ignore(10000, '\n'); cout << prompt; }
    cin.ignore(10000, '\n');
    return v;
}
static string readLine(const string& prompt) {
    cout << prompt;
    string s; getline(cin, s);
    return s;
}

static vector<int> readMemberList(const string& prompt) {
    cout << prompt << " (comma separated user IDs): ";
    string line; getline(cin, line);
    vector<int> ids;
    for (auto& tok : Storage::splitStr(line, ','))
        if (!tok.empty()) ids.push_back(stoi(tok));
    return ids;
}

// ------------------------------------------------------------------
// MAIN  (menu-driven CLI)
// ------------------------------------------------------------------
int main() {
    ExpenseSplitter app;
    cout << "===== Expense Splitter (Splitwise-style) =====\n";

    while (true) {
        cout << "\n============ MENU ============\n"
             << " 1. Register User\n"
             << " 2. Create Group\n"
             << " 3. Add Member to Group\n"
             << " 4. Add Expense\n"
             << " 5. View Balances\n"
             << " 6. Settle Up (min transactions)\n"
             << " 7. View Expense History\n"
             << " 8. List Users\n"
             << " 9. List Groups\n"
             << " 0. Save & Exit\n"
             << "===============================\n";
        int choice = readInt("Choice: ");

        if (choice == 1) {
            string name = readLine("Name: ");
            string email = readLine("Email: ");
            int id = app.registerUser(name, email);
            cout << "User registered with ID " << id << "\n";

        } else if (choice == 2) {
            string name = readLine("Group name: ");
            int id = app.createGroup(name);
            cout << "Group created with ID " << id << "\n";

        } else if (choice == 3) {
            app.listGroups(); app.listUsers();
            int gid = readInt("Group ID: ");
            int uid = readInt("User ID: ");
            cout << (app.addMemberToGroup(gid, uid) ? "Member added.\n" : "Failed (invalid IDs).\n");

        } else if (choice == 4) {
            app.listGroups();
            int gid = readInt("Group ID: ");
            Group* g = app.findGroup(gid);
            if (!g) { cout << "Group not found.\n"; continue; }

            string desc = readLine("Description: ");
            double amount = readDouble("Amount: ");
            int paidBy = readInt("Paid by (User ID): ");

            cout << "Split type -> 1.Equal 2.Exact 3.Percentage: ";
            int st; cin >> st; cin.ignore(10000, '\n');
            SplitType type = st == 2 ? SplitType::EXACT : st == 3 ? SplitType::PERCENT : SplitType::EQUAL;

            vector<int> participants = readMemberList("Participants");
            unordered_map<int, double> input;
            if (type == SplitType::EXACT) {
                for (int p : participants)
                    input[p] = readDouble("  Amount owed by user " + to_string(p) + ": ");
            } else if (type == SplitType::PERCENT) {
                for (int p : participants)
                    input[p] = readDouble("  Percentage for user " + to_string(p) + ": ");
            }

            bool ok = app.addExpense(gid, desc, amount, paidBy, type, input, participants);
            cout << (ok ? "Expense added.\n" : "Failed to add expense (check members/amounts).\n");

        } else if (choice == 5) {
            app.listGroups();
            int gid = readInt("Group ID: ");
            app.printBalances(gid);

        } else if (choice == 6) {
            app.listGroups();
            int gid = readInt("Group ID: ");
            app.printSettlement(gid);

        } else if (choice == 7) {
            app.listGroups();
            int gid = readInt("Group ID: ");
            app.printExpenseHistory(gid);

        } else if (choice == 8) {
            app.listUsers();

        } else if (choice == 9) {
            app.listGroups();

        } else if (choice == 0) {
            app.save();
            cout << "Data saved. Goodbye!\n";
            break;
        } else {
            cout << "Invalid choice.\n";
        }
    }
    return 0;
}
