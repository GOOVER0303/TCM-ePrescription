// ============================================================
//  题目2  中医电子处方编辑系统
//  数据结构：双向链栈（Undo 栈 / Redo 栈，节点存“操作命令”对象）
//  编译：g++ main.cpp -o prescription -std=c++11
// ============================================================
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

using namespace std;

// ----------------------- 基础数据结构 -----------------------

// 单味药（常用方 / 处方内条目）
struct HerbItem {
    string name;      // 药物名称
    string spec;      // 规格
    double dosage;    // 用量(g)
};

// 中药档案
struct Medicine {
    string code;      // 编号
    string name;      // 名称
    string origin;    // 产地
    string spec;      // 规格
};

// 药量限制
struct DoseLimit {
    string name;      // 药物名称
    string spec;      // 规格
    double maxDose;   // 最大用量(g)
};

// 常用方（验方 / 经方）
struct Preset {
    int seq;                      // 序号
    string name;                  // 方名
    vector<HerbItem> herbs;       // 药方
};

// 处方状态
enum PState { DRAFT, SUBMITTED, REVOKED };

// 一张处方
struct Prescription {
    string id;                    // 处方编号
    string doctor;                // 医生姓名
    int doses;                    // 剂数
    vector<HerbItem> herbs;       // 药物
    string signature;             // 医生签名（超量确认后自动加入）
    PState state;                 // DRAFT / SUBMITTED / REVOKED
};

// ----------------------- 全局数据（文件持久化） -----------------------
vector<Medicine>  medicines;
vector<DoseLimit> limits;
vector<Preset>    presets;
vector<Prescription> prescriptions;

static const string MED_FILE = "medicines.dat";
static const string LIM_FILE = "limits.dat";
static const string PRE_FILE = "presets.dat";
static const string RX_FILE  = "prescriptions.dat";

// ----------------------- 命令对象（存入链栈节点） -----------------------
// 命令模式：每个可撤销操作封装为一个命令对象，携带 undo / redo 所需数据

enum CmdType { CMD_ADD_RX, CMD_DEL_RX, CMD_EDIT_RX, CMD_SUBMIT_RX, CMD_REVOKE_RX, CMD_RESTORE_RX };

struct Command {
    CmdType type;
    int rxIndex;                       // 作用的处方下标
    Prescription before;               // 操作前快照（撤销用）
    Prescription after;                // 操作后快照（重做用）
};

// 双向链栈节点
struct StackNode {
    Command cmd;
    StackNode* prev;
    StackNode* next;
};

// 双向链栈：top 为栈顶；入栈时清空 redo
class CommandStack {
public:
    StackNode* top = nullptr;
    int size = 0;

    void push(const Command& c) {
        clearRedo();                    // 新操作作废已有的重做历史
        StackNode* n = new StackNode();
        n->cmd = c; n->prev = top; n->next = nullptr;
        if (top) top->next = n;
        top = n; size++;
    }

    Command pop() {
        StackNode* n = top;
        Command c = n->cmd;
        top = n->prev;
        if (top) top->next = nullptr;
        delete n; size--;
        return c;
    }

    void pushRedo(const Command& c) {
        StackNode* n = new StackNode();
        n->cmd = c; n->prev = top; n->next = nullptr;
        if (top) top->next = n;
        top = n; size++;
    }

    bool empty() { return top == nullptr; }

private:
    void clearRedo() {
        while (redoTop) {
            StackNode* n = redoTop; redoTop = redoTop->prev;
            if (redoTop) redoTop->next = nullptr;
            delete n;
        }
        redoSize = 0;
    }
public:
    StackNode* redoTop = nullptr;
    int redoSize = 0;

    void pushToRedo(const Command& c) {
        StackNode* n = new StackNode();
        n->cmd = c; n->prev = redoTop; n->next = nullptr;
        if (redoTop) redoTop->next = n;
        redoTop = n; redoSize++;
    }

    Command popRedo() {
        StackNode* n = redoTop;
        Command c = n->cmd;
        redoTop = n->prev;
        if (redoTop) redoTop->next = nullptr;
        delete n; redoSize--;
        return c;
    }

    bool redoEmpty() { return redoTop == nullptr; }
};

static CommandStack history;

// ----------------------- 工具函数 -----------------------

string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// 查找中药档案是否存在
bool medicineExists(const string& name) {
    for (auto& m : medicines)
        if (m.name == name) return true;
    return false;
}

// 查最大用量限制；未配置返回 -1（不受限）
double getMaxDose(const string& name) {
    for (auto& l : limits)
        if (l.name == name) return l.maxDose;
    return -1;
}

// ----------------------- 规范检测 -----------------------
// 返回 0=通过；1=药名不存在；2=剂量超标待确认
int validateHerb(const HerbItem& h, string& msg) {
    if (!medicineExists(h.name)) {
        msg = "药名【" + h.name + "】不在中药档案中，请核对！";
        return 1;
    }
    double mx = getMaxDose(h.name);
    if (mx > 0 && h.dosage > mx) {
        ostringstream os;
        os << "【" << h.name << "】用量 " << h.dosage << "g 超过最大限制 " << mx << "g，需医生确认签名！";
        msg = os.str();
        return 2;
    }
    return 0;
}

// ----------------------- 持久化 -----------------------

void saveAll() {
    ofstream f(MED_FILE);
    for (auto& m : medicines)
        f << m.code << "|" << m.name << "|" << m.origin << "|" << m.spec << "\n";
    f.close();

    ofstream g(LIM_FILE);
    for (auto& l : limits)
        g << l.name << "|" << l.spec << "|" << l.maxDose << "\n";
    g.close();

    ofstream p(PRE_FILE);
    for (auto& r : presets) {
        p << r.seq << "|" << r.name;
        for (auto& h : r.herbs) p << "|" << h.name << "|" << h.spec << "|" << h.dosage;
        p << "\n";
    }
    p.close();

    ofstream x(RX_FILE);
    for (auto& rx : prescriptions) {
        x << rx.id << "|" << rx.doctor << "|" << rx.doses << "|"
          << rx.signature << "|" << rx.state;
        for (auto& h : rx.herbs) x << "|" << h.name << "|" << h.spec << "|" << h.dosage;
        x << "\n";
    }
    x.close();
}

void loadAll() {
    ifstream f(MED_FILE); string line;
    while (getline(f, line)) {
        stringstream ss(line); string tok; Medicine m;
        getline(ss, m.code, '|'); getline(ss, m.name, '|');
        getline(ss, m.origin, '|'); getline(ss, m.spec, '|');
        medicines.push_back(m);
    }
    f.close();

    ifstream g(LIM_FILE);
    while (getline(g, line)) {
        stringstream ss(line); string tok; DoseLimit l;
        getline(ss, l.name, '|'); getline(ss, l.spec, '|');
        ss >> l.maxDose; limits.push_back(l);
    }
    g.close();

    ifstream p(PRE_FILE);
    while (getline(p, line)) {
        stringstream ss(line); Preset r; string tok;
        getline(ss, tok, '|'); r.seq = stoi(tok); getline(ss, r.name, '|');
        while (getline(ss, tok, '|')) {
            HerbItem h; h.name = tok;
            getline(ss, h.spec, '|'); getline(ss, tok, '|');
            h.dosage = stod(tok); r.herbs.push_back(h);
        }
        presets.push_back(r);
    }
    p.close();

    ifstream x(RX_FILE);
    while (getline(x, line)) {
        stringstream ss(line); Prescription rx; string tok;
        getline(ss, rx.id, '|'); getline(ss, rx.doctor, '|');
        getline(ss, tok, '|'); rx.doses = stoi(tok);
        getline(ss, rx.signature, '|'); getline(ss, tok, '|');
        rx.state = (PState)stoi(tok);
        while (getline(ss, tok, '|')) {
            HerbItem h; h.name = tok;
            getline(ss, h.spec, '|'); getline(ss, tok, '|');
            h.dosage = stod(tok); rx.herbs.push_back(h);
        }
        prescriptions.push_back(rx);
    }
    x.close();
}

// ----------------------- 中药 / 药量管理 -----------------------

void manageMedicines() {
    int op;
    while (true) {
        cout << "\n===== 中药信息管理 =====\n1.新增 2.修改 3.删除 4.返回\n请选择：";
        cin >> op;
        if (op == 4) break;
        if (op == 1) {
            Medicine m; cout << "编号 名称 产地 规格：";
            cin >> m.code >> m.name >> m.origin >> m.spec;
            medicines.push_back(m);
        } else if (op == 2) {
            string name; cout << "要修改的药名："; cin >> name;
            for (auto& m : medicines) if (m.name == name) {
                cout << "新编号 名称 产地 规格："; cin >> m.code >> m.name >> m.origin >> m.spec;
            }
        } else if (op == 3) {
            string name; cout << "要删除的药名："; cin >> name;
            medicines.erase(remove_if(medicines.begin(), medicines.end(),
                [&](const Medicine& m){ return m.name == name; }), medicines.end());
        }
        saveAll();
    }
}

void manageLimits() {
    int op;
    while (true) {
        cout << "\n===== 药量控制管理 =====\n1.新增 2.修改 3.删除 4.返回\n请选择：";
        cin >> op;
        if (op == 4) break;
        if (op == 1) {
            DoseLimit l; cout << "药名 规格 最大用量(g)："; cin >> l.name >> l.spec >> l.maxDose;
            limits.push_back(l);
        } else if (op == 2) {
            string name; cout << "要修改的药名："; cin >> name;
            for (auto& l : limits) if (l.name == name) { cout << "规格 最大用量："; cin >> l.spec >> l.maxDose; }
        } else if (op == 3) {
            string name; cout << "要删除的药名："; cin >> name;
            limits.erase(remove_if(limits.begin(), limits.end(),
                [&](const DoseLimit& l){ return l.name == name; }), limits.end());
        }
        saveAll();
    }
}

// ----------------------- 常用方管理 -----------------------

void managePresets() {
    int op;
    while (true) {
        cout << "\n===== 常用方管理 =====\n1.新增 2.修改 3.删除 4.选择此方加入当前处方 5.返回\n请选择：";
        cin >> op;
        if (op == 5) break;
        if (op == 1) {
            Preset r; cout << "序号 方名："; cin >> r.seq >> r.name;
            int n; cout << "药物味数："; cin >> n;
            for (int i = 0; i < n; i++) {
                HerbItem h; cout << "药名 规格 用量："; cin >> h.name >> h.spec >> h.dosage;
                r.herbs.push_back(h);
            }
            presets.push_back(r);
        } else if (op == 2) {
            int seq; cout << "要修改的序号："; cin >> seq;
            for (auto& r : presets) if (r.seq == seq) {
                r.herbs.clear(); int n; cout << "新药味数："; cin >> n;
                for (int i = 0; i < n; i++) {
                    HerbItem h; cin >> h.name >> h.spec >> h.dosage; r.herbs.push_back(h);
                }
            }
        } else if (op == 3) {
            int seq; cout << "要删除的序号："; cin >> seq;
            presets.erase(remove_if(presets.begin(), presets.end(),
                [&](const Preset& r){ return r.seq == seq; }), presets.end());
        } else if (op == 4) {
            int seq; cout << "要加入的序号："; cin >> seq;
            for (auto& r : presets) if (r.seq == seq) {
                // 复用方内容到一张新草稿
                Prescription rx;
                rx.id = "RX" + to_string(prescriptions.size() + 1);
                rx.herbs = r.herbs; rx.state = DRAFT;
                prescriptions.push_back(rx);
                cout << "已将【" << r.name << "】加入新处方 " << rx.id << "\n";
            }
        }
        saveAll();
    }
}

// ----------------------- 处方操作（配合命令栈） -----------------------

void undo() {
    if (history.empty()) { cout << "无可撤销操作\n"; return; }
    Command c = history.pop();
    history.pushToRedo(c);
    switch (c.type) {
        case CMD_ADD_RX:
            prescriptions.erase(prescriptions.begin() + c.rxIndex);
            cout << "撤销：新增处方已回退\n"; break;
        case CMD_EDIT_RX:
            prescriptions[c.rxIndex] = c.before;
            cout << "撤销：处方修改已回退\n"; break;
        case CMD_SUBMIT_RX:
            prescriptions[c.rxIndex] = c.before;
            cout << "撤销：提交已回退为草稿\n"; break;
        case CMD_REVOKE_RX:
            prescriptions[c.rxIndex] = c.before;
            cout << "撤销：撤销操作已恢复为提交态\n"; break;
        case CMD_RESTORE_RX:
            prescriptions[c.rxIndex] = c.before;
            cout << "撤销：恢复操作已回退\n"; break;
        case CMD_DEL_RX:
            prescriptions.insert(prescriptions.begin() + c.rxIndex, c.before);
            cout << "撤销：删除的处方已找回\n"; break;
    }
    saveAll();
}

void redo() {
    if (history.redoEmpty()) { cout << "无可重做操作\n"; return; }
    Command c = history.popRedo();
    history.push(c);
    switch (c.type) {
        case CMD_ADD_RX:
            prescriptions.insert(prescriptions.begin() + c.rxIndex, c.after);
            break;
        case CMD_EDIT_RX:
        case CMD_SUBMIT_RX:
        case CMD_REVOKE_RX:
        case CMD_RESTORE_RX:
            prescriptions[c.rxIndex] = c.after; break;
        case CMD_DEL_RX:
            prescriptions.erase(prescriptions.begin() + c.rxIndex); break;
    }
    cout << "已重做操作\n";
    saveAll();
}

void newPrescription() {
    Prescription rx;
    rx.id = "RX" + to_string(prescriptions.size() + 1);
    cout << "医生姓名："; cin >> rx.doctor;
    cout << "剂数："; cin >> rx.doses;
    int n; cout << "药物味数："; cin >> n;
    bool overLimit = false;
    for (int i = 0; i < n; i++) {
        HerbItem h; string msg;
        cout << "药名 规格 用量："; cin >> h.name >> h.spec >> h.dosage;
        int v = validateHerb(h, msg);
        if (v == 1) { cout << msg << "（已拦截）\n"; continue; }
        if (v == 2) {
            cout << msg << "\n是否超量确认(y/n)："; char y; cin >> y;
            if (y == 'y' || y == 'Y') { rx.signature = rx.doctor; overLimit = true; }
            else continue;
        }
        rx.herbs.push_back(h);
    }
    rx.state = DRAFT;
    Command c; c.type = CMD_ADD_RX; c.rxIndex = prescriptions.size(); c.after = rx;
    history.push(c);
    prescriptions.push_back(rx);
    cout << "草稿处方 " << rx.id << " 已建立" << (overLimit ? "（含超量签名）" : "") << "\n";
    saveAll();
}

void submitPrescription() {
    string id; cout << "提交处方编号："; cin >> id;
    for (size_t i = 0; i < prescriptions.size(); i++) {
        if (prescriptions[i].id == id) {
            if (prescriptions[i].state == SUBMITTED) { cout << "已提交，不可修改\n"; return; }
            Prescription before = prescriptions[i];
            prescriptions[i].state = SUBMITTED;
            Command c; c.type = CMD_SUBMIT_RX; c.rxIndex = i; c.before = before; c.after = prescriptions[i];
            history.push(c);
            cout << "处方已提交，提交后不能修改\n";
            saveAll(); return;
        }
    }
    cout << "未找到该处方\n";
}

void editPrescription() {
    string id; cout << "修改处方编号："; cin >> id;
    for (size_t i = 0; i < prescriptions.size(); i++) {
        if (prescriptions[i].id == id) {
            if (prescriptions[i].state == SUBMITTED) { cout << "已提交，不可修改；请先撤销\n"; return; }
            Prescription before = prescriptions[i];
            cout << "新剂数："; cin >> prescriptions[i].doses;
            Command c; c.type = CMD_EDIT_RX; c.rxIndex = i; c.before = before; c.after = prescriptions[i];
            history.push(c);
            cout << "修改完成\n"; saveAll(); return;
        }
    }
}

void deletePrescription() {
    string id; cout << "删除处方编号："; cin >> id;
    for (size_t i = 0; i < prescriptions.size(); i++) {
        if (prescriptions[i].id == id) {
            if (prescriptions[i].state == SUBMITTED) { cout << "已提交，请先撤销\n"; return; }
            Command c; c.type = CMD_DEL_RX; c.rxIndex = i; c.before = prescriptions[i];
            history.push(c);
            prescriptions.erase(prescriptions.begin() + i);
            cout << "已删除（可撤销找回）\n"; saveAll(); return;
        }
    }
}

void revokePrescription() {
    string id; cout << "撤销处方编号（仅限本人提交）："; cin >> id;
    string doc; cout << "当前医生姓名："; cin >> doc;
    for (size_t i = 0; i < prescriptions.size(); i++) {
        if (prescriptions[i].id == id) {
            if (prescriptions[i].doctor != doc) { cout << "非本人处方，无权撤销\n"; return; }
            if (prescriptions[i].state != SUBMITTED) { cout << "仅已提交处方可撤销\n"; return; }
            Prescription before = prescriptions[i];
            prescriptions[i].state = REVOKED;
            Command c; c.type = CMD_REVOKE_RX; c.rxIndex = i; c.before = before; c.after = prescriptions[i];
            history.push(c);
            cout << "处方已撤销\n"; saveAll(); return;
        }
    }
}

void restorePrescription() {
    string id; cout << "恢复处方编号："; cin >> id;
    for (size_t i = 0; i < prescriptions.size(); i++) {
        if (prescriptions[i].id == id) {
            if (prescriptions[i].state != REVOKED) { cout << "仅撤销态可恢复\n"; return; }
            Prescription before = prescriptions[i];
            prescriptions[i].state = DRAFT;   // 恢复后可修改，需重新提交
            Command c; c.type = CMD_RESTORE_RX; c.rxIndex = i; c.before = before; c.after = prescriptions[i];
            history.push(c);
            cout << "处方已恢复为草稿，可修改后重新提交\n"; saveAll(); return;
        }
    }
}

void listPrescriptions() {
    cout << "\n----- 处方列表 -----\n";
    for (auto& rx : prescriptions) {
        cout << rx.id << " | 医生:" << rx.doctor << " | " << rx.doses << "剂 | "
             << (rx.state == DRAFT ? "草稿" : rx.state == SUBMITTED ? "已提交" : "已撤销")
             << " | 签名:" << (rx.signature.empty() ? "无" : rx.signature) << "\n";
        for (auto& h : rx.herbs)
            cout << "    " << h.name << " " << h.spec << " " << h.dosage << "g\n";
    }
}

// ----------------------- 主菜单 -----------------------

int main() {
    loadAll();
    int op;
    while (true) {
        cout << "\n========== 中医电子处方编辑系统 ==========\n"
             << "1.常用方管理  2.中药信息管理  3.药量控制管理\n"
             << "4.开具处方    5.修改处方      6.删除处方\n"
             << "7.提交处方    8.撤销处方      9.恢复处方\n"
             << "10.撤销一步   11.重做一步    12.查看处方  0.退出\n"
             << "请选择：";
        cin >> op;
        switch (op) {
            case 1: managePresets(); break;
            case 2: manageMedicines(); break;
            case 3: manageLimits(); break;
            case 4: newPrescription(); break;
            case 5: editPrescription(); break;
            case 6: deletePrescription(); break;
            case 7: submitPrescription(); break;
            case 8: revokePrescription(); break;
            case 9: restorePrescription(); break;
            case 10: undo(); break;
            case 11: redo(); break;
            case 12: listPrescriptions(); break;
            case 0: saveAll(); cout << "数据已保存，再见\n"; return 0;
            default: cout << "无效选择\n";
        }
    }
}
