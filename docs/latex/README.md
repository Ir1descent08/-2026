# docs/latex

这个目录用于整理原创功能相关的 LaTeX 文档源文件，方便后续生成课程报告或单独导出 PDF。

## 文件说明

- `main.tex`
  - 总入口文件
  - 定义文档类、常用宏、目录和子文档引用
  - 后续生成完整文档时，优先从这个文件编译

- `cover.tex`
  - 封面页内容
  - 包含课程名称、项目名称、学生姓名、学号、指导教师、提交日期等字段
  - 这些字段由 `main.tex` 中定义的宏控制

- `reaction_game_original_feature.tex`
  - 原创功能“反应时间测试游戏”的正文
  - 采用 `subfiles` 方式接入 `main.tex`
  - 内容包括动机、设计、协议、实现、演示和总结

- `Makefile`
  - 简单编译入口
  - 默认使用 `xelatex` 两次编译 `main.tex`
  - 输出目录为 `build/`

## 编译方式

### 方式一：直接用 xelatex

在当前目录执行：

```bash
xelatex -interaction=nonstopmode -halt-on-error main.tex
xelatex -interaction=nonstopmode -halt-on-error main.tex
```

如果希望把输出放到 `build/` 目录：

```bash
mkdir -p build
xelatex -interaction=nonstopmode -halt-on-error -output-directory build main.tex
xelatex -interaction=nonstopmode -halt-on-error -output-directory build main.tex
```

### 方式二：使用 Makefile

在当前目录执行：

```bash
make pdf
```

清理输出文件：

```bash
make clean
```

## 使用建议

- 平时编辑正文时，优先改 `reaction_game_original_feature.tex`
- 如果需要修改封面信息，改 `main.tex` 中的宏定义或 `cover.tex` 的排版
- 如果后续还要补更多章节，可以继续新增 `.tex` 子文档，再在 `main.tex` 中通过 `\subfile{...}` 引入

## 依赖说明

建议本地安装支持中文的 TeX 环境，并使用 `xelatex` 编译。

如果本机没有安装 `xelatex`，则无法直接生成 PDF，但不影响继续维护这些源文件。