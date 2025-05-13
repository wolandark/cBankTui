#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <ncurses.h>

sqlite3 *db;

void initDB() {
    if (sqlite3_open("bank.db", &db)) {
        endwin();
        fprintf(stderr, "DB error: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS accounts ("
        "accountNumber INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "balance REAL NOT NULL DEFAULT 0);";

    char *err = 0;
    if (sqlite3_exec(db, sql, 0, 0, &err) != SQLITE_OK) {
        endwin();
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
        exit(1);
    }
}

void printAccounts(WINDOW *win) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT accountNumber, name, balance FROM accounts";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    werase(win);
    mvwprintw(win, 1, 2, "Account Number | Name                 | Balance");
    mvwprintw(win, 2, 2, "----------------------------------------------");
    int row = 3;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int acc = sqlite3_column_int(stmt, 0);
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        double bal = sqlite3_column_double(stmt, 2);

        mvwprintw(win, row++, 2, "%14d | %-20s | %.2f", acc, name, bal);
    }

    sqlite3_finalize(stmt);
    box(win, 0, 0);
    wrefresh(win);
}

void promptInput(const char *prompt, char *buf, int maxlen) {
    echo();
    mvprintw(LINES - 2, 2, "%s", prompt);
    clrtoeol();
    getnstr(buf, maxlen);
    noecho();
}

void createAccount() {
    char name[50];
    promptInput("Enter name: ", name, 49);

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO accounts (name, balance) VALUES (?, 0.0)";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        mvprintw(LINES - 3, 2, "Account created successfully.");
    else
        mvprintw(LINES - 3, 2, "Failed to create account.");

    sqlite3_finalize(stmt);
}

void depositOrWithdraw(int isDeposit) {
    char input[50];
    int acc;
    float amt;

    promptInput("Account number: ", input, 49);
    acc = atoi(input);

    promptInput(isDeposit ? "Deposit amount: " : "Withdraw amount: ", input, 49);
    amt = atof(input);
    if (amt <= 0) return;

    sqlite3_stmt *stmt;
    const char *checkSql = "SELECT balance FROM accounts WHERE accountNumber = ?";
    sqlite3_prepare_v2(db, checkSql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, acc);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        mvprintw(LINES - 3, 2, "Invalid account.");
        sqlite3_finalize(stmt);
        return;
    }

    double bal = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);

    if (!isDeposit && amt > bal) {
        mvprintw(LINES - 3, 2, "Insufficient funds.");
        return;
    }

    const char *sql = isDeposit ?
        "UPDATE accounts SET balance = balance + ? WHERE accountNumber = ?" :
        "UPDATE accounts SET balance = balance - ? WHERE accountNumber = ?";

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_double(stmt, 1, amt);
    sqlite3_bind_int(stmt, 2, acc);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        mvprintw(LINES - 3, 2, isDeposit ? "Deposit successful." : "Withdrawal successful.");
    else
        mvprintw(LINES - 3, 2, "Operation failed.");

    sqlite3_finalize(stmt);
}

void drawMenu(const char **options, int count, int selected) {
    for (int i = 0; i < count; i++) {
        if (i == selected) {
            attron(A_REVERSE);
            mvprintw(LINES - 5 + i, 2, "%s", options[i]);
            attroff(A_REVERSE);
        } else {
            mvprintw(LINES - 5 + i, 2, "%s", options[i]);
        }
    }
    refresh();
}

void runTUI() {
    const char *options[] = {
        "Create Account",
        "Deposit",
        "Withdraw",
        "Refresh Table",
        "Quit"
    };
    const int n_options = sizeof(options) / sizeof(options[0]);
    int selected = 0;

    WINDOW *table = newwin(LINES - 8, COLS - 4, 1, 2);
    keypad(stdscr, TRUE);
    curs_set(0);

    while (1) {
        printAccounts(table);
        drawMenu(options, n_options, selected);
        mvprintw(LINES - 1, 2, "Use arrows ↑↓ and Enter to select.");
        refresh();

        int ch = getch();
        if (ch == KEY_UP) {
            selected = (selected - 1 + n_options) % n_options;
        } else if (ch == KEY_DOWN) {
            selected = (selected + 1) % n_options;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            clear();
            printAccounts(table);
            switch (selected) {
                case 0: createAccount(); break;
                case 1: depositOrWithdraw(1); break;
                case 2: depositOrWithdraw(0); break;
                case 3: break; // refresh only
                case 4: return;
            }
        }
    }
}

int main() {
    initscr();
    noecho();
    cbreak();
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);

    initDB();
    runTUI();

    sqlite3_close(db);
    endwin();
    return 0;
}
