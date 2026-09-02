// Minimal rolling time-series line chart on a <canvas>, no dependencies.
// Not a general-purpose charting library - just enough to plot one series
// of (time, value) samples pulled straight from F' telemetry as they arrive.
export class TimeSeriesChart {
  // canvas: the <canvas> element to draw into
  // color: CSS color string for the line
  // windowSeconds: how much history to keep/show
  // options.autoscale: if true (default), the y-axis rescales to the
  //   window's current min/max each draw; if false, options.yMin/yMax are fixed
  constructor(canvas, color, windowSeconds, options = {}) {
    this.ctx = canvas.getContext("2d");
    this.canvas = canvas;
    this.color = color;
    this.windowSeconds = windowSeconds;
    this.autoscale = options.autoscale ?? true;
    this.yMin = options.yMin ?? 0;
    this.yMax = options.yMax ?? 1;
    this.samples = []; // [{t, v}, ...], t in seconds, oldest first
  }

  // t: seconds (any monotonically-increasing clock, e.g. performance.now()/1000)
  push(t, value) {
    this.samples.push({ t, v: value });
    const cutoff = t - this.windowSeconds;
    while (this.samples.length > 1 && this.samples[0].t < cutoff) {
      this.samples.shift();
    }
  }

  draw() {
    const { ctx, canvas } = this;
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    if (this.samples.length < 2) {
      return;
    }

    const latestT = this.samples[this.samples.length - 1].t;
    const earliestT = latestT - this.windowSeconds;

    let yMin = this.yMin;
    let yMax = this.yMax;
    if (this.autoscale) {
      yMin = Math.min(...this.samples.map((s) => s.v));
      yMax = Math.max(...this.samples.map((s) => s.v));
      if (yMax - yMin < 1e-6) {
        // Flat series - pad the range so the line doesn't render on the edge
        yMin -= 0.5;
        yMax += 0.5;
      }
    }

    const x = (t) => ((t - earliestT) / this.windowSeconds) * w;
    const y = (v) => h - ((v - yMin) / (yMax - yMin)) * h;

    ctx.strokeStyle = this.color;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    this.samples.forEach((s, i) => {
      const px = x(s.t);
      const py = y(s.v);
      if (i === 0) {
        ctx.moveTo(px, py);
      } else {
        ctx.lineTo(px, py);
      }
    });
    ctx.stroke();
  }
}
