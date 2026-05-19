from __future__ import annotations

import csv
import shutil
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from math import log10
from statistics import mean, median

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.ticker import PercentFormatter


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"
FIGURES = ROOT / "figures"
RUN_DIR = FIGURES / f"run_{datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}"
plt.rcParams["font.family"] = "DejaVu Sans"
PVALUE_DISPLAY_FLOOR = 1e-12


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def save(fig, name: str) -> None:
    RUN_DIR.mkdir(parents=True, exist_ok=True)
    path = RUN_DIR / name
    fig.tight_layout()
    fig.savefig(path, dpi=180, bbox_inches="tight")
    plt.close(fig)
    print(f"[plots] wrote: {path}")


def set_log_xscale(ax) -> None:
    ax.set_xscale("log")


def set_noise_xscale(ax, sigmas) -> None:
    if max(sigmas, default=0.0) <= 2.0:
        ax.set_xscale("linear")
    else:
        ax.set_xscale("symlog", linthresh=0.25)

    if len(sigmas) > 25 and max(sigmas, default=0.0) <= 2.0:
        ticks = [0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0]
    elif len(sigmas) > 25:
        ticks = [sigma for sigma in sigmas if sigma == 0.0 or abs(sigma * 2 - round(sigma * 2)) < 1e-9]
    else:
        ticks = sigmas

    ax.set_xticks(ticks)
    ax.set_xticklabels([f"{sigma:g}" for sigma in ticks], fontsize=8)
    ax.tick_params(axis="x", labelrotation=35)


def set_symlog_yscale(ax, linthresh: float = 1.0) -> None:
    ax.set_yscale("symlog", linthresh=linthresh)


def set_positive_log_yscale(ax) -> None:
    ax.set_yscale("log")


def set_pvalue_yscale(ax, values: list[float], alpha: float = 1e-3) -> bool:
    finite = [value for value in values if value == value and value > 0.0]
    if not finite:
        ax.set_yscale("log")
        return True

    minimum = min(finite)
    maximum = max(finite)
    ratio = maximum / minimum if minimum > 0.0 else float("inf")

    if minimum >= 0.03 and ratio <= 30.0:
        padding = max(0.02, 0.18 * (maximum - minimum))
        ax.set_yscale("linear")
        ax.set_ylim(max(0.0, minimum - padding), min(1.02, maximum + padding))
        return False

    ax.set_yscale("log")
    lower = max(1e-300, min(minimum, alpha) * 0.6)
    upper = min(1.2, max(maximum, alpha) * 1.8)
    ax.set_ylim(lower, upper)
    return True


def add_alpha_reference(ax, alpha: float = 1e-3) -> None:
    ymin, ymax = ax.get_ylim()
    if ymin <= alpha <= ymax:
        ax.axhline(
            alpha,
            color="#d00000",
            linestyle="--",
            linewidth=1.2,
            label="alpha = 1e-3\nПорог alpha = 1e-3",
        )
        return

    ax.text(
        0.985,
        0.045,
        "alpha = 1e-3 below scale\nalpha = 1e-3 ниже шкалы",
        transform=ax.transAxes,
        ha="right",
        va="bottom",
        fontsize=8,
        color="#d00000",
    )


def set_bilingual_labels(
    ax,
    title_en: str,
    title_ru: str,
    xlabel_en: str,
    xlabel_ru: str,
    ylabel_en: str,
    ylabel_ru: str,
) -> None:
    ax.set_title(title_en, fontsize=13, pad=28)
    ax.text(
        0.5,
        1.035,
        title_ru,
        transform=ax.transAxes,
        ha="center",
        va="bottom",
        fontsize=10,
    )
    ax.set_xlabel(xlabel_en, fontsize=10)
    ax.xaxis.set_label_coords(0.5, -0.08)
    ax.text(
        0.5,
        -0.15,
        xlabel_ru,
        transform=ax.transAxes,
        ha="center",
        va="top",
        fontsize=8,
    )
    ax.set_ylabel(ylabel_en, fontsize=10)
    ax.yaxis.set_label_coords(-0.11, 0.5)
    ax.text(
        -0.16,
        0.5,
        ylabel_ru,
        transform=ax.transAxes,
        ha="center",
        va="center",
        rotation=90,
        fontsize=8,
    )


def set_percent_axis(ax) -> None:
    ax.set_ylim(0.0, 110.0)
    ax.set_yticks([0, 20, 40, 60, 80, 100])
    ax.yaxis.set_major_formatter(PercentFormatter(xmax=100.0, decimals=0))


def fmt_percent(value: float) -> str:
    return f"{value:.0f}%"


def fmt_integer(value: float) -> str:
    return f"{value:.0f}"


def fmt_runtime(value: float) -> str:
    if value >= 100.0:
        return f"{value:.0f} ms"
    if value >= 10.0:
        return f"{value:.1f} ms"
    return f"{value:.2f} ms"


def fmt_plain(value: float) -> str:
    if abs(value) >= 1000.0:
        return f"{value:.1e}"
    if abs(value - round(value)) < 1e-9:
        return f"{value:.0f}"
    return f"{value:.2g}"


def fmt_scientific(value: float) -> str:
    if 0.0 < value <= PVALUE_DISPLAY_FLOOR * 1.000001:
        return "<1e-12"
    if value <= 1e-299:
        return "<1e-299"
    if abs(value) >= 1e4 or 0 < abs(value) < 1e-2:
        return f"{value:.1e}"
    return f"{value:.2g}"


def display_pvalues(values: list[float]) -> list[float]:
    return [max(PVALUE_DISPLAY_FLOOR, value) for value in values]


def is_dense(xs) -> bool:
    return len(xs) > 50


def line_style(xs, *, smooth: bool = False) -> dict:
    if is_dense(xs):
        return {
            "marker": None,
            "linewidth": 1.55 if smooth else 0.9,
            "alpha": 0.95 if smooth else 0.75,
        }

    return {
        "marker": "o",
        "markersize": 4.5,
        "linewidth": 1.8,
        "alpha": 1.0,
    }


def smooth_series(values: list[float], window: int = 11, *, log_space: bool = False) -> list[float]:
    if len(values) < window:
        return values

    half = window // 2
    smoothed = []
    for index in range(len(values)):
        start = max(0, index - half)
        end = min(len(values), index + half + 1)
        local = [value for value in values[start:end] if value == value]
        if not local:
            smoothed.append(float("nan"))
        elif log_space:
            smoothed.append(10 ** mean(log10(max(1e-300, value)) for value in local))
        else:
            smoothed.append(mean(local))
    return smoothed


def maybe_smooth(values: list[float], smooth: bool, *, log_space: bool = False) -> list[float]:
    return smooth_series(values, log_space=log_space) if smooth else values


def suffix(name: str, smooth: bool) -> str:
    if not smooth:
        return name
    stem, ext = name.rsplit(".", 1)
    return f"{stem}_smooth.{ext}"


def key_sigma_values(sigmas: list[float]) -> list[float]:
    if len(sigmas) <= 25:
        return sigmas

    keys = [0.0, 0.25, 0.5, 1.0, 1.5, 2.0]
    return [
        sigma for sigma in sigmas
        if any(abs(sigma - key) < 1e-9 for key in keys)
    ]


def annotate_points(
    ax,
    xs,
    ys,
    formatter,
    dy: int = 7,
    color: str | None = None,
    dense_keys: set[float] | None = None,
) -> None:
    label_xs = None
    if len(xs) > 25:
        key_values = dense_keys or {0.0, 0.25, 0.5, 1.0, 1.5, 2.0}
        label_xs = {
            x for x in xs
            if any(abs(float(x) - key) < 1e-9 for key in key_values)
        }

    for x, y in zip(xs, ys):
        if label_xs is not None and x not in label_xs:
            continue

        ax.annotate(
            formatter(y),
            (x, y),
            textcoords="offset points",
            xytext=(0, dy),
            ha="center",
            fontsize=7,
            color=color,
        )


def grouped_rows(rows, *keys):
    groups = defaultdict(list)
    for row in rows:
        groups[tuple(row[key] for key in keys)].append(row)
    return groups


def median_float(rows, column: str) -> float:
    values = [
        float(row[column])
        for row in rows
        if row[column] not in {"", "nan", "NaN"}
    ]
    if not values:
        return float("nan")
    return median(values)


def copy_inputs() -> None:
    RUN_DIR.mkdir(parents=True, exist_ok=True)
    for path in [
        RESULTS / "noise_robustness.csv",
        RESULTS / "catalog_presence_detection.csv",
        RESULTS / "runtime_catalog_size.csv",
        RESULTS / "orbit_type_robustness.csv",
    ]:
        if not path.exists():
            continue
        target = RUN_DIR / path.name
        shutil.copy2(path, target)
        print(f"[plots] copied: {target}")


def plot_noise_robustness() -> None:
    rows = read_csv(RESULTS / "noise_robustness.csv")
    sigmas = sorted({float(row["sigma_deg"]) for row in rows})
    modes = [mode for mode in ["prefix", "hard"] if any(row.get("catalog_mode") == mode for row in rows)]
    if not modes:
        modes = [""]

    by_sigma_mode = grouped_rows(rows, "sigma_deg", "catalog_mode") if "catalog_mode" in rows[0] else None

    top1_by_mode = {}
    for mode in modes:
        values = []
        for sigma in sigmas:
            if by_sigma_mode is None:
                items = [row for row in rows if float(row["sigma_deg"]) == sigma]
            else:
                items = by_sigma_mode[(f"{sigma:g}", mode)]
                if not items:
                    items = [
                        row for row in rows
                        if float(row["sigma_deg"]) == sigma
                        and row.get("catalog_mode") == mode
                    ]
            values.append(
                100.0 * mean(int(row["top1_correct"]) for row in items)
                if items else float("nan")
            )
        top1_by_mode[mode] = values

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(9.5, 5.4))
        for mode in modes:
            top1_accuracy = maybe_smooth(top1_by_mode[mode], smooth)
            label = {
                "prefix": "Top-1, prefix catalog\nTop-1, обычный каталог",
                "hard": "Top-1, hard-negative catalog\nTop-1, hard-negative каталог",
                "": "Top-1 exact target\nTop-1: точный спутник",
            }[mode]
            ax.plot(sigmas, top1_accuracy, label=label, **line_style(sigmas, smooth=smooth))
            annotate_points(ax, sigmas, top1_accuracy, fmt_percent, dy=7 if mode == "prefix" else -15)

        set_bilingual_labels(
            ax,
            "Top-1 degradation under angular noise" + (" (smoothed)" if smooth else ""),
            "Падение top-1 точности при росте углового шума" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Top-1 success rate",
            "Доля точных top-1 решений",
        )
        set_percent_axis(ax)
        set_noise_xscale(ax, sigmas)
        ax.grid(True, which="major", alpha=0.3)
        ax.legend(fontsize=8)
        save(fig, suffix("noise_success_rate.png", smooth))

    hard_rows = [row for row in rows if row.get("catalog_mode", "hard") == "hard"]
    if not hard_rows:
        hard_rows = rows
    hard_grouped: dict[float, list[dict[str, str]]] = defaultdict(list)
    for row in hard_rows:
        hard_grouped[float(row["sigma_deg"])].append(row)

    sigmas_hard = sorted(hard_grouped)
    top3_accuracy = []
    top10_accuracy = []
    median_rank = []
    median_gap = []
    ambiguity = []

    for sigma in sigmas_hard:
        items = hard_grouped[sigma]
        top3_accuracy.append(100.0 * mean(int(row["top3_contains_target"]) for row in items))
        top10_accuracy.append(100.0 * mean(int(row["top10_contains_target"]) for row in items))
        median_rank.append(median(int(row["target_rank"]) for row in items))
        median_gap.append(median(float(row["best_gap_chi2"]) for row in items))
        ambiguity.append(median(int(row["ambiguous_best_count"]) for row in items))

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(9.5, 5.4))
        top3_plot = maybe_smooth(top3_accuracy, smooth)
        top10_plot = maybe_smooth(top10_accuracy, smooth)
        ax.plot(sigmas_hard, top3_plot, label="Top-3 contains target\nTarget входит в top-3", **line_style(sigmas_hard, smooth=smooth))
        ax.plot(sigmas_hard, top10_plot, label="Top-10 contains target\nTarget входит в top-10", **line_style(sigmas_hard, smooth=smooth))
        set_bilingual_labels(
            ax,
            "Near-miss robustness in hard-negative catalog" + (" (smoothed)" if smooth else ""),
            "Устойчивость near-miss в hard-negative каталоге" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Success rate",
            "Доля успешных решений",
        )
        set_percent_axis(ax)
        set_noise_xscale(ax, sigmas_hard)
        ax.grid(True, which="major", alpha=0.3)
        ax.legend(fontsize=8)
        annotate_points(ax, sigmas_hard, top3_plot, fmt_percent, dy=7)
        annotate_points(ax, sigmas_hard, top10_plot, fmt_percent, dy=-15)
        save(fig, suffix("noise_topk_hard.png", smooth))

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(9.5, 5.4))
        values = maybe_smooth(median_rank, smooth)
        ax.plot(sigmas_hard, values, color="#7b2cbf", **line_style(sigmas_hard, smooth=smooth))
        set_bilingual_labels(
            ax,
            "Target rank in hard-negative catalog" + (" (smoothed)" if smooth else ""),
            "Ранг target в hard-negative каталоге" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Median target rank",
            "Медианный ранг target",
        )
        set_noise_xscale(ax, sigmas_hard)
        ax.set_ylim(0, max(values) + 1)
        ax.grid(True, which="major", alpha=0.3)
        annotate_points(ax, sigmas_hard, values, fmt_integer)
        save(fig, suffix("noise_target_rank.png", smooth))

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(9.5, 5.4))
        values = maybe_smooth(median_gap, smooth, log_space=True)
        ax.plot(sigmas_hard, values, color="#2a9d8f", **line_style(sigmas_hard, smooth=smooth))
        set_bilingual_labels(
            ax,
            "Separation between best and second-best candidates" + (" (smoothed)" if smooth else ""),
            "Разделение между первым и вторым кандидатом" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Median chi-square gap",
            "Медианный разрыв по chi-square",
        )
        set_noise_xscale(ax, sigmas_hard)
        set_positive_log_yscale(ax)
        ax.grid(True, which="both", alpha=0.3)
        annotate_points(ax, sigmas_hard, values, fmt_scientific)
        save(fig, suffix("noise_chi2_gap.png", smooth))

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(9.5, 5.4))
        values = maybe_smooth(ambiguity, smooth)
        ax.plot(sigmas_hard, values, color="#f77f00", **line_style(sigmas_hard, smooth=smooth))
        set_bilingual_labels(
            ax,
            "Ambiguity of the best solution" + (" (smoothed)" if smooth else ""),
            "Неоднозначность лучшего решения" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Median equivalent best candidates",
            "Медианное число эквивалентно лучших кандидатов",
        )
        set_noise_xscale(ax, sigmas_hard)
        ax.set_ylim(0, max(values) + 1)
        ax.grid(True, which="major", alpha=0.3)
        annotate_points(ax, sigmas_hard, values, fmt_integer)
        save(fig, suffix("noise_ambiguity.png", smooth))


def plot_catalog_presence() -> None:
    rows = read_csv(RESULTS / "catalog_presence_detection.csv")
    alphas = [5e-2, 1e-2, 1e-3, 1e-4, 1e-6]
    default_alpha = 1e-3

    sigmas = sorted({float(row["sigma_deg"]) for row in rows})
    scenario_order = [
        "known_hard",
        "unknown_random",
        "unknown_hard",
        "known",
        "unknown",
    ]
    scenarios = [
        scenario for scenario in scenario_order
        if any(row["scenario"] == scenario for row in rows)
    ]

    grouped: dict[tuple[str, float], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[(row["scenario"], float(row["sigma_deg"]))].append(row)

    labels = {
        "known_hard": "Known hard: accepted\nKnown hard: принят",
        "unknown_random": "Unknown random: rejected\nUnknown random: отвергнут",
        "unknown_hard": "Unknown hard: rejected\nUnknown hard: отвергнут",
        "known": "Known: accepted\nЕсть в каталоге: принят",
        "unknown": "Unknown: rejected\nНет в каталоге: отвергнут",
    }

    pvalue_labels = {
        "known_hard": "Known hard\nTarget есть, hard каталог",
        "unknown_random": "Unknown random\nTarget удален, обычный каталог",
        "unknown_hard": "Unknown hard\nTarget удален, hard каталог",
        "known": "Known target\nTarget есть в каталоге",
        "unknown": "Target removed\nTarget удален из каталога",
    }

    success_by_scenario = {}
    for scenario in scenarios:
        success = []
        for sigma in sigmas:
            items = grouped[(scenario, sigma)]
            if scenario.startswith("known"):
                success.append(
                    100.0 * mean(
                        float(row["best_fit_p_value"]) >= default_alpha
                        for row in items
                    )
                    if items else float("nan")
                )
            else:
                success.append(
                    100.0 * mean(
                        float(row["best_fit_p_value"]) < default_alpha
                        for row in items
                    )
                    if items else float("nan")
                )
        success_by_scenario[scenario] = success

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(9.5, 5.4))
        for index, scenario in enumerate(scenarios):
            success = maybe_smooth(success_by_scenario[scenario], smooth)
            ax.plot(sigmas, success, label=labels[scenario], **line_style(sigmas, smooth=smooth))
            annotate_points(ax, sigmas, success, fmt_percent, dy=7 + 8 * (index % 3))

        set_bilingual_labels(
            ax,
            "Catalog presence detection (alpha = 1e-3)" + (" (smoothed)" if smooth else ""),
            "Проверка наличия спутника в каталоге (alpha = 1e-3)" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Decision success rate",
            "Доля правильных open-set решений",
        )
        set_percent_axis(ax)
        set_noise_xscale(ax, sigmas)
        ax.grid(True, which="major", alpha=0.3)
        ax.legend(fontsize=8)
        save(fig, suffix("catalog_presence_success.png", smooth))

    pvalues_by_scenario = {
        scenario: [
            max(1e-300, median_float(grouped[(scenario, sigma)], "best_fit_p_value"))
            for sigma in sigmas
        ]
        for scenario in scenarios
    }

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(9.5, 5.4))
        plotted_pvalues = []
        for index, scenario in enumerate(scenarios):
            pvalues = maybe_smooth(pvalues_by_scenario[scenario], smooth, log_space=True)
            display_values = display_pvalues(pvalues)
            plotted_pvalues.extend(display_values)
            ax.plot(sigmas, display_values, label=pvalue_labels[scenario], **line_style(sigmas, smooth=smooth))
            annotate_points(ax, sigmas, display_values, fmt_scientific, dy=7 if index % 2 == 0 else -15)
        set_pvalue_yscale(ax, plotted_pvalues, default_alpha)
        add_alpha_reference(ax, default_alpha)
        set_bilingual_labels(
            ax,
            "Best candidate goodness-of-fit" + (" (smoothed)" if smooth else ""),
            "Согласованность лучшего кандидата с наблюдениями" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Median best-candidate fit p-value",
            "Медианное p-value лучшего кандидата",
        )
        set_noise_xscale(ax, sigmas)
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(fontsize=8)
        save(fig, suffix("catalog_presence_pvalue.png", smooth))

    fig, ax = plt.subplots(figsize=(9.5, 5.4))
    for sigma in key_sigma_values(sigmas):
        items = [row for row in rows if float(row["sigma_deg"]) == sigma]
        balanced_accuracy = []
        for alpha in alphas:
            known = [row for row in items if row["scenario"].startswith("known")]
            unknown = [row for row in items if row["scenario"].startswith("unknown")]
            known_rate = mean(
                float(row["best_fit_p_value"]) >= alpha
                for row in known
            )
            unknown_rate = mean(
                float(row["best_fit_p_value"]) < alpha
                for row in unknown
            )
            balanced_accuracy.append(100.0 * 0.5 * (known_rate + unknown_rate))

        ax.plot(
            alphas,
            balanced_accuracy,
            marker="o",
            label=f"sigma={sigma:g} deg\nsigma={sigma:g} град",
        )
        annotate_points(ax, alphas, balanced_accuracy, fmt_percent, dy=7)

    ax.axvline(default_alpha, color="#d00000", linestyle="--", label="selected alpha\nвыбранный alpha")
    ax.set_xscale("log")
    ax.invert_xaxis()
    set_bilingual_labels(
        ax,
        "Open-set threshold sensitivity",
        "Чувствительность open-set решения к порогу",
        "Decision alpha",
        "Порог принятия решения alpha",
        "Balanced accuracy",
        "Сбалансированная точность",
    )
    set_percent_axis(ax)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=8, ncols=2)
    save(fig, "catalog_presence_alpha_sweep.png")


def plot_runtime() -> None:
    rows = read_csv(RESULTS / "runtime_catalog_size.csv")
    grouped: dict[int, list[float]] = defaultdict(list)

    for row in rows:
        grouped[int(row["catalog_size"])].append(float(row["runtime_ms"]))

    sizes = sorted(grouped)
    avg_ms = [mean(grouped[size]) for size in sizes]
    per_sat = [avg / size for avg, size in zip(avg_ms, sizes)]

    fig, ax = plt.subplots(figsize=(9.5, 5.4))
    ax.plot(sizes, avg_ms, marker="o", color="#006d77")
    set_bilingual_labels(
        ax,
        "Core runtime vs catalog size",
        "Время работы ядра в зависимости от размера каталога",
        "Catalog size, satellites",
        "Размер каталога, спутники",
        "Runtime, ms",
        "Время работы, мс",
    )
    set_log_xscale(ax)
    set_positive_log_yscale(ax)
    ax.grid(True, which="both", alpha=0.3)
    annotate_points(ax, sizes, avg_ms, fmt_runtime)
    save(fig, "runtime_catalog_size.png")

    fig, ax = plt.subplots(figsize=(9.5, 5.4))
    ax.plot(sizes, per_sat, marker="o", color="#d00000")
    set_bilingual_labels(
        ax,
        "Runtime per satellite",
        "Время работы в пересчете на один спутник",
        "Catalog size, satellites",
        "Размер каталога, спутники",
        "Runtime per satellite, ms",
        "Время на один спутник, мс",
    )
    set_log_xscale(ax)
    ax.grid(True, which="major", alpha=0.3)
    annotate_points(ax, sizes, per_sat, fmt_runtime)
    save(fig, "runtime_per_satellite.png")


def plot_orbit_type_robustness() -> None:
    path = RESULTS / "orbit_type_robustness.csv"
    if not path.exists():
        print(f"[plots] optional missing: {path}")
        return

    rows = read_csv(path)
    if not rows:
        print(f"[plots] optional empty: {path}")
        return

    orbit_order = ["LEO", "MEO", "GEO", "Polar", "Elliptical"]
    orbit_types = [
        orbit_type for orbit_type in orbit_order
        if any(row["orbit_type"] == orbit_type for row in rows)
    ]
    sigmas = sorted({float(row["sigma_deg"]) for row in rows})

    grouped: dict[tuple[str, float], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[(row["orbit_type"], float(row["sigma_deg"]))].append(row)

    top1_by_type = {}
    top10_by_type = {}
    rank_by_type = {}
    pvalue_by_type = {}

    for orbit_type in orbit_types:
        top1_values = []
        top10_values = []
        rank_values = []
        pvalue_values = []
        for sigma in sigmas:
            items = grouped[(orbit_type, sigma)]
            top1_values.append(
                100.0 * mean(int(row["top1_correct"]) for row in items)
                if items else float("nan")
            )
            top10_values.append(
                100.0 * mean(int(row["top10_contains_target"]) for row in items)
                if items else float("nan")
            )
            ranks = [int(row["target_rank"]) for row in items if int(row["target_rank"]) > 0]
            rank_values.append(median(ranks) if ranks else float("nan"))
            pvalue_values.append(max(1e-300, median_float(items, "best_fit_p_value")))

        top1_by_type[orbit_type] = top1_values
        top10_by_type[orbit_type] = top10_values
        rank_by_type[orbit_type] = rank_values
        pvalue_by_type[orbit_type] = pvalue_values

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(10.5, 5.8))
        for index, orbit_type in enumerate(orbit_types):
            values = maybe_smooth(top1_by_type[orbit_type], smooth)
            ax.plot(sigmas, values, label=f"{orbit_type}\n{orbit_type}", **line_style(sigmas, smooth=smooth))
            annotate_points(
                ax,
                sigmas,
                values,
                fmt_percent,
                dy=7 + 7 * (index % 3),
                dense_keys={2.0},
            )

        set_bilingual_labels(
            ax,
            "Top-1 accuracy by orbit type" + (" (smoothed)" if smooth else ""),
            "Top-1 точность по типам орбит" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Top-1 success rate",
            "Доля точных top-1 решений",
        )
        set_percent_axis(ax)
        set_noise_xscale(ax, sigmas)
        ax.grid(True, which="major", alpha=0.3)
        ax.legend(fontsize=8, ncols=2)
        save(fig, suffix("orbit_type_top1_accuracy.png", smooth))

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(10.5, 5.8))
        for index, orbit_type in enumerate(orbit_types):
            values = maybe_smooth(top10_by_type[orbit_type], smooth)
            ax.plot(sigmas, values, label=f"{orbit_type}\n{orbit_type}", **line_style(sigmas, smooth=smooth))
            annotate_points(
                ax,
                sigmas,
                values,
                fmt_percent,
                dy=7 + 7 * (index % 3),
                dense_keys={2.0},
            )

        set_bilingual_labels(
            ax,
            "Top-10 containment by orbit type" + (" (smoothed)" if smooth else ""),
            "Попадание target в top-10 по типам орбит" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Top-10 contains target",
            "Target входит в top-10",
        )
        set_percent_axis(ax)
        set_noise_xscale(ax, sigmas)
        ax.grid(True, which="major", alpha=0.3)
        ax.legend(fontsize=8, ncols=2)
        save(fig, suffix("orbit_type_top10_accuracy.png", smooth))

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(10.5, 5.8))
        for index, orbit_type in enumerate(orbit_types):
            values = maybe_smooth(rank_by_type[orbit_type], smooth)
            ax.plot(sigmas, values, label=f"{orbit_type}\n{orbit_type}", **line_style(sigmas, smooth=smooth))
            annotate_points(
                ax,
                sigmas,
                values,
                fmt_integer,
                dy=7 + 7 * (index % 3),
                dense_keys={2.0},
            )

        set_bilingual_labels(
            ax,
            "Median target rank by orbit type" + (" (smoothed)" if smooth else ""),
            "Медианный ранг target по типам орбит" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Median target rank",
            "Медианный ранг target",
        )
        set_noise_xscale(ax, sigmas)
        ax.grid(True, which="major", alpha=0.3)
        ax.legend(fontsize=8, ncols=2)
        save(fig, suffix("orbit_type_target_rank.png", smooth))

    for smooth in [False, True]:
        fig, ax = plt.subplots(figsize=(10.5, 5.8))
        plotted_pvalues = []
        for index, orbit_type in enumerate(orbit_types):
            values = maybe_smooth(pvalue_by_type[orbit_type], smooth, log_space=True)
            display_values = display_pvalues(values)
            plotted_pvalues.extend(display_values)
            ax.plot(sigmas, display_values, label=f"{orbit_type}\n{orbit_type}", **line_style(sigmas, smooth=smooth))
            annotate_points(
                ax,
                sigmas,
                display_values,
                fmt_scientific,
                dy=7 if index % 2 == 0 else -15,
                dense_keys={2.0},
            )

        set_pvalue_yscale(ax, plotted_pvalues, 1e-3)
        add_alpha_reference(ax, 1e-3)
        set_bilingual_labels(
            ax,
            "Best-candidate p-value by orbit type" + (" (smoothed)" if smooth else ""),
            "p-value лучшего кандидата по типам орбит" + (" (сглажено)" if smooth else ""),
            "Angular noise sigma, deg",
            "СКО углового шума, градусы",
            "Median best-candidate p-value",
            "Медианное p-value лучшего кандидата",
        )
        set_noise_xscale(ax, sigmas)
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(fontsize=8, ncols=2)
        save(fig, suffix("orbit_type_pvalue.png", smooth))

    runtime_by_type = []
    labels = []
    for orbit_type in orbit_types:
        items = [row for row in rows if row["orbit_type"] == orbit_type]
        runtime_by_type.append(median(float(row["runtime_ms"]) for row in items))
        labels.append(orbit_type)

    fig, ax = plt.subplots(figsize=(9.5, 5.4))
    bars = ax.bar(labels, runtime_by_type, color="#006d77")
    set_bilingual_labels(
        ax,
        "Median runtime by orbit type",
        "Медианное время обработки по типам орбит",
        "Orbit type",
        "Тип орбиты",
        "Runtime, ms",
        "Время работы, мс",
    )
    ax.grid(True, axis="y", alpha=0.3)
    for bar, value in zip(bars, runtime_by_type):
        ax.annotate(
            fmt_runtime(value),
            (bar.get_x() + bar.get_width() / 2.0, value),
            textcoords="offset points",
            xytext=(0, 7),
            ha="center",
            fontsize=8,
        )
    save(fig, "orbit_type_runtime.png")


def main() -> None:
    required = [
        RESULTS / "noise_robustness.csv",
        RESULTS / "catalog_presence_detection.csv",
        RESULTS / "runtime_catalog_size.csv",
    ]

    missing = [path for path in required if not path.exists()]
    if missing:
        for path in missing:
            print(f"[plots] missing: {path}")
        raise SystemExit("Run C++ experiments first")

    print(f"[plots] output directory: {RUN_DIR}")

    copy_inputs()
    plot_noise_robustness()
    plot_catalog_presence()
    plot_orbit_type_robustness()
    plot_runtime()


if __name__ == "__main__":
    main()
