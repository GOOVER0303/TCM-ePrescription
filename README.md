# TCM-ePrescription

中医电子处方编辑系统 —— 数据结构课程设计（题目 2）。

以 **双向链栈（Undo 栈 / Redo 栈，节点存“操作命令”对象）** 为核心的处方撤销 / 重做机制，
实现常用方管理、处方开具与规范检测、处方撤销恢复、中药档案与药量限制管理，全部数据以文件持久化。

## 功能

| 模块 | 说明 |
| --- | --- |
| 常用方管理 | 常用方的增加、修改、删除；选择某方后将整方内容带入当前处方 |
| 处方开具 | 录入药名、规格、用量、剂数、医生姓名；药名校验、剂量超标签名确认；新增 / 修改 / 删除 / 提交（提交后锁定不可改） |
| 处方撤销 / 恢复 | 撤销仅限医生本人提交的处方；恢复后回到草稿态，可修改并需重新提交 |
| 中药信息管理 | 中药档案的增加、修改、删除（药物名称、编号、产地、规格） |
| 药量控制 | 限量药物的增加、修改、删除（药物名称、规格、最大用量） |
| 撤销 / 重做 | 任意可回退操作入 Undo 栈，撤销后进入 Redo 栈，可反向重做 |

## 编译与运行

```bash
g++ main.cpp -o prescription -std=c++11
./prescription
```

Windows（MinGW / MSVC）：

```bash
g++ main.cpp -o prescription.exe -std=c++11
prescription.exe
```

## 主菜单

```
1.常用方管理   2.中药信息管理   3.药量控制管理
4.开具处方     5.修改处方       6.删除处方
7.提交处方     8.撤销处方       9.恢复处方
10.撤销一步    11.重做一步      12.查看处方     0.退出
```

## 数据结构

```cpp
struct HerbItem     { name; spec; dosage; }        // 单味药
struct Medicine     { code; name; origin; spec; }  // 中药档案
struct DoseLimit    { name; spec; maxDose; }       // 药量限制
struct Preset       { seq; name; vector<HerbItem> herbs; }
struct Prescription { id; doctor; doses; herbs; signature; state; }
enum   PState       { DRAFT, SUBMITTED, REVOKED }

struct Command {                                   // 操作命令对象
    CmdType type;        // 新增 / 删除 / 修改 / 提交 / 撤销 / 恢复
    int rxIndex;         // 处方下标
    Prescription before; // 操作前快照（撤销用）
    Prescription after;  // 操作后快照（重做用）
};
struct StackNode { Command cmd; StackNode* prev; StackNode* next; };
```

撤销：Undo 栈弹出 → 按 `before` 回退 → 压入 Redo 栈。
重做：Redo 栈弹出 → 按 `after` 还原 → 压回 Undo 栈。
产生新操作时清空 Redo 历史。

## 处方状态流转

```
草稿 DRAFT --提交--> 已提交 SUBMITTED --撤销(限本人)--> 已撤销 REVOKED
   ^                                                          |
   +---------------------- 恢复（需重新提交） -----------------+
```

## 数据文件

程序启动时加载、退出时保存，均为 `|` 分隔的纯文本：

| 文件 | 内容 |
| --- | --- |
| `medicines.dat` | 编号 \| 名称 \| 产地 \| 规格 |
| `limits.dat` | 名称 \| 规格 \| 最大用量 |
| `presets.dat` | 序号 \| 方名 \| (药名 \| 规格 \| 用量)×n |
| `prescriptions.dat` | 编号 \| 医生 \| 剂数 \| 签名 \| 状态 \| (药名 \| 规格 \| 用量)×n |

状态取值：`0` = 草稿，`1` = 已提交，`2` = 已撤销。

仓库内附带一组样例数据（25 味中药、6 条药量限制、4 首经方、2 张处方），可直接运行演示。

## 复杂度

| 操作 | 实现 | 复杂度 |
| --- | --- | --- |
| 撤销 / 重做 | 双向链栈顶操作 | O(1) |
| 药名 / 限量校验 | 向量顺序查找 | O(n) |
| 档案新增 | `push_back` | O(1) 均摊 |
| 档案修改 / 删除 | 查找 + 元素移动 | O(n) |
