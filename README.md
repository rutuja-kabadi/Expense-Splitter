# Expense-Splitter
An expense splitting app that allows groups to track shared costs, settle debts, and minimize transactions
# 💸 Expense Management System (Splitwise-Clone)

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![Architecture](https://img.shields.io/badge/Architecture-Monolithic_Backend-success.svg)
![Storage](https://img.shields.io/badge/Storage-File_I/O-orange.svg)

A robust, terminal-based backend application written in **C++17** to manage users, groups, and shared expenses. This project implements advanced algorithmic debt simplification (min-cash-flow) and features a custom file-persistence engine to maintain relational application state without external databases.

## 🚀 Key Features

* **Debt Simplification Engine:** Engineered a two-pointer greedy algorithm to optimize group debt settlements; mathematically reduces complex, multi-directional debt graphs into $\le N-1$ simplified transactions in **O(N)** time.
* **Dynamic Expense Splitting:** Implemented modular split strategies (**Equal, Exact, Percent**) with strict epsilon-based tolerance limits to guarantee 100% precision in floating-point ledger calculations.
* **Optimized State Management:** Extensively leverages C++17 features including **R-value references (`std::move`)** and **Hash Maps (`std::unordered_map`)** to achieve **O(1)** average time complexity for user state and balance lookups.
* **Custom Persistence Pipeline:** Designed a robust state-persistence engine via C++ File I/O (`fstream`) to serialize relational data models (Users, Groups, Transactions) into pipe-delimited text files across sessions.
* **Clean Architecture:** Applied the **Facade Design Pattern** to strictly decouple disk-write operations from core business logic, adhering to the Single Responsibility Principle (SRP).

---

## 🧠 Algorithmic Highlight: Min-Cash Flow Optimization

When multiple people owe each other money, the debt graph becomes incredibly complex. Instead of mapping every single transaction historically, this system utilizes a **Greedy Algorithm** to settle debts efficiently:
1. Calculates the net balance for every user in the group.
2. Segregates users into two lists: **Creditors** (positive net balance) and **Debtors** (negative net balance).
3. Iteratively settles the maximum possible amount between the top Debtor and top Creditor using `min(debt, credit)`.
4. Guarantees that all debts are settled in the absolute minimum number of transactions possible.

---

## 🛠️ Tech Stack & Concepts

* **Language:** C++17
* **Core Libraries:** Standard Template Library (STL), `<fstream>`, `<unordered_map>`, `<vector>`
* **Design Patterns:** Facade Pattern, Object-Oriented Programming (OOP)
* **Algorithms:** Greedy Two-Pointer Approach

---

## 💻 Getting Started

### Prerequisites
You need a C++ compiler that supports C++17 (e.g., GCC or Clang).

### Installation & Execution
1. **Clone the repository:**
   ```bash
   git clone [https://github.com/YourUsername/ExpenseSplitter.git](https://github.com/YourUsername/ExpenseSplitter.git)
   cd ExpenseSplitter
