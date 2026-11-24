import { describe, expect, it } from "vitest";
import { lineChart } from "../chart";

describe("lineChart", () => {
  it("lineChart renders one path per series and a marker per Belady point", () => {
    const svg = lineChart(
      [
        { name: "FIFO", values: [12, 12, 9, 10, 5] },
        { name: "LRU", values: [12, 12, 10, 8, 5] },
      ],
      [1, 2, 3, 4, 5],
      { markers: [{ series: "FIFO", x: 4, label: "anomalia de Belady" }], title: "Faults por frames" },
    );
    expect(svg.querySelectorAll("path.series")).toHaveLength(2);
    const markers = svg.querySelectorAll(".marker");
    expect(markers).toHaveLength(1);
    expect(markers[0].textContent).toContain("anomalia de Belady");
    expect(svg.querySelector("title")?.textContent).toBe("Faults por frames");
    expect(svg.getAttribute("role")).toBe("img");
    // the legend names every series
    expect(svg.textContent).toContain("FIFO");
    expect(svg.textContent).toContain("LRU");
  });

  it("handles a single point and all-zero values without NaN", () => {
    const svg = lineChart([{ name: "OPT", values: [0] }], [1]);
    expect(svg.innerHTML).not.toContain("NaN");
  });
});
