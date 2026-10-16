"""ShopEasy: a simple online shopping screen.
All the sorting, searching and recommending is done by the C program (product_recommender.c).
Run:  python gui.py      (keep both files in the same folder; gcc must be installed)
"""
import math
import os
import subprocess
import sys
import tkinter as tk
from tkinter import ttk, messagebox

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)
SRC = os.path.join(HERE, "product_recommender.c")
EXE = os.path.join(HERE, "recommender.exe" if os.name == "nt" else "recommender")

FONT = "Segoe UI" if os.name == "nt" else "Helvetica"
MONO = ("Consolas" if os.name == "nt" else "Courier", 10)
BG, NAVY, CARD, GREY = "#f3f4f6", "#1e3a8a", "#ffffff", "#6b7280"
ICON = {"Phone": "📱", "Headphone": "🎧", "Laptop": "💻", "Watch": "⌚"}
TINT = {"Phone": "#dbeafe", "Headphone": "#fce7f3", "Laptop": "#dcfce7", "Watch": "#fef3c7"}
SORTS = {"Featured": (4, 1), "Price: Low to High": (1, 1), "Price: High to Low": (1, 2),
         "Customer Rating": (2, 2), "Name: A to Z": (3, 1)}
FEATS = ["Wireless", "Waterproof", "FastCharge", "Bluetooth", "5G", "Touchscreen", "NoiseCancel", "Eco"]
MAP_COLORS = {0: "#2563eb", 1: "#16a34a", 2: "#f59e0b", 3: "#ef4444", -1: "#cbd5e1"}


def build_c_program():
    if os.path.exists(EXE) and os.path.getmtime(EXE) >= os.path.getmtime(SRC):
        return
    r = subprocess.run(["gcc", SRC, "-o", EXE, "-lm"], capture_output=True, text=True)
    if r.returncode != 0:
        messagebox.showerror("Build failed", "Could not compile the C program with gcc.\n\n" + r.stderr)
        sys.exit(1)


def nice(s):
    return s.replace("_", " ")


def money(v):
    return "₹{:,.0f}".format(float(v))


def stars(r):
    k = int(round(float(r)))
    return "★" * k + "☆" * (5 - k) + "  %.1f" % float(r)


def prod(f):
    return {"id": int(f[0]), "name": f[1], "cat": f[2], "brand": f[3],
            "price": float(f[4]), "rating": float(f[5]), "feat": f[6]}


def rows(lines):
    out = []
    for l in lines:
        f = l.split("\t")
        if len(f) == 7 and f[0].isdigit():
            out.append(prod(f))
    return out


class Scroll(tk.Frame):
    """A frame that scrolls up and down."""
    def __init__(self, parent):
        super().__init__(parent, bg=BG)
        self.canvas = tk.Canvas(self, bg=BG, highlightthickness=0)
        sb = ttk.Scrollbar(self, command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.canvas.pack(side="left", fill="both", expand=True)
        self.inner = tk.Frame(self.canvas, bg=BG)
        self.win = self.canvas.create_window((0, 0), window=self.inner, anchor="nw")
        self.inner.bind("<Configure>", lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all")))
        self.canvas.bind("<Configure>", lambda e: self.canvas.itemconfig(self.win, width=e.width))
        self.canvas.bind("<Enter>", lambda e: self.canvas.bind_all("<MouseWheel>", self.wheel))

    def wheel(self, e):
        self.canvas.yview_scroll(-2 if e.delta > 0 else 2, "units")

    def clear(self):
        for w in self.inner.winfo_children():
            w.destroy()
        self.canvas.yview_moveto(0)


def make_clickable(w, fn):
    w.bind("<Button-1>", lambda e: fn())
    try:
        w.configure(cursor="hand2")
    except tk.TclError:
        pass
    for c in w.winfo_children():
        make_clickable(c, fn)


class App:
    def __init__(self, root):
        self.root = root
        root.title("ShopEasy")
        root.geometry("1130x720")
        root.configure(bg=BG)
        ttk.Style().theme_use("clam")
        self.threshold = tk.IntVar(value=45)
        self.alg_var = tk.IntVar(value=1)          # 1 = Merge Sort, 2 = Quick Sort (developer mode)
        self.info = tk.StringVar(value="")
        self.search_var = tk.StringVar()
        self.budget_var = tk.StringVar(value="")
        self.brand_var = tk.StringVar(value="")
        self.by_id = {}
        self.current = 1
        self.dev = None

        head = tk.Frame(root, bg=NAVY)
        head.pack(fill="x")
        tk.Label(head, text="🛍  ShopEasy", bg=NAVY, fg="white", font=(FONT, 18, "bold")).pack(side="left", padx=16, pady=10)
        e = tk.Entry(head, textvariable=self.search_var, font=(FONT, 12), width=38, relief="flat")
        e.pack(side="left", ipady=6, padx=(20, 6))
        e.bind("<Return>", lambda ev: self.go_shop())
        tk.Button(head, text="Search", font=(FONT, 11, "bold"), bg="#f59e0b", relief="flat",
                  padx=14, command=self.go_shop).pack(side="left")
        tk.Button(head, text="⚙ Developer mode", font=(FONT, 9), bg=NAVY, fg="#bfdbfe", relief="flat",
                  command=self.open_dev).pack(side="right", padx=12)

        bar = tk.Frame(root, bg="white")
        bar.pack(fill="x")
        pad = {"padx": 6, "pady": 8}
        tk.Label(bar, text="Category", bg="white", font=(FONT, 10)).pack(side="left", padx=(14, 2))
        self.cat_cb = ttk.Combobox(bar, width=16, state="readonly", values=["All categories"])
        self.cat_cb.current(0)
        self.cat_cb.pack(side="left", **pad)
        tk.Label(bar, text="Price  ₹", bg="white", font=(FONT, 10)).pack(side="left", padx=(14, 2))
        self.p_min = ttk.Entry(bar, width=8)
        self.p_min.pack(side="left")
        tk.Label(bar, text="to", bg="white").pack(side="left", padx=4)
        self.p_max = ttk.Entry(bar, width=8)
        self.p_max.pack(side="left")
        tk.Button(bar, text="Apply", relief="flat", bg="#e5e7eb", command=self.apply_filters).pack(side="left", padx=8)
        tk.Label(bar, text="Sort by", bg="white", font=(FONT, 10)).pack(side="left", padx=(20, 2))
        self.sort_cb = ttk.Combobox(bar, width=18, state="readonly", values=list(SORTS))
        self.sort_cb.current(0)
        self.sort_cb.pack(side="left", **pad)
        tk.Button(bar, text="Clear all", relief="flat", bg="#e5e7eb", command=self.reset).pack(side="left", padx=10)
        self.cat_cb.bind("<<ComboboxSelected>>", self.apply_filters)
        self.sort_cb.bind("<<ComboboxSelected>>", self.apply_filters)

        self.pages = tk.Frame(root, bg=BG)
        self.pages.pack(fill="both", expand=True)
        self.shop = Scroll(self.pages)
        self.detail = Scroll(self.pages)
        self.reload()

    # talking to the C program
    def run(self, *args):
        r = subprocess.run([EXE, str(self.threshold.get())] + [str(a) for a in args],
                           capture_output=True, text=True)
        return r.stdout.splitlines()

    def show_page(self, page):
        self.shop.pack_forget()
        self.detail.pack_forget()
        page.pack(fill="both", expand=True)

    def reload(self):
        items = rows(self.run("list"))
        self.by_id = {p["id"]: p for p in items}
        self.cat_cb["values"] = ["All categories"] + sorted({p["cat"] for p in items})
        self.apply_filters()

    def reset(self):
        self.search_var.set("")
        self.p_min.delete(0, "end")
        self.p_max.delete(0, "end")
        self.cat_cb.current(0)
        self.sort_cb.current(0)
        self.go_shop()

    def go_shop(self):
        self.apply_filters()

    # product cards
    def make_card(self, parent, p, tag="", tag_color="#16a34a"):
        c = tk.Frame(parent, bg=CARD, width=240, height=315 if tag else 275, highlightbackground="#d1d5db", highlightthickness=1)
        c.pack_propagate(False)
        tk.Label(c, text=ICON.get(p["cat"], "🛍"), font=(FONT, 34), bg=TINT.get(p["cat"], "#e5e7eb")).pack(fill="x", ipady=10)
        tk.Label(c, text=nice(p["name"]), font=(FONT, 12, "bold"), bg=CARD, anchor="w").pack(fill="x", padx=10, pady=(8, 0))
        tk.Label(c, text="%s  •  %s" % (p["brand"], p["cat"]), font=(FONT, 9), bg=CARD, fg=GREY, anchor="w").pack(fill="x", padx=10)
        tk.Label(c, text=stars(p["rating"]), font=(FONT, 10), bg=CARD, fg="#f59e0b", anchor="w").pack(fill="x", padx=10)
        tk.Label(c, text=money(p["price"]), font=(FONT, 14, "bold"), bg=CARD, fg="#065f46", anchor="w").pack(fill="x", padx=10)
        feats = p["feat"].split(", ")
        tk.Label(c, text=", ".join(feats[:2]) + (" +%d more" % (len(feats) - 2) if len(feats) > 2 else ""),
                 font=(FONT, 9), bg=CARD, fg=GREY, anchor="w", justify="left", wraplength=215).pack(fill="x", padx=10, pady=(0, 6))
        if tag:
            tk.Label(c, text=tag, font=(FONT, 9, "bold"), bg=tag_color, fg="white", wraplength=215).pack(fill="x", padx=10, pady=(0, 10))
        make_clickable(c, lambda i=p["id"]: self.show_product(i))
        return c

    def place_cards(self, parent, items, cols=4):
        f = tk.Frame(parent, bg=BG)
        f.pack(fill="x", padx=14)
        for k, (p, tag, col) in enumerate(items):
            self.make_card(f, p, tag, col).grid(row=k // cols, column=k % cols, padx=7, pady=7, sticky="n")
        for k in range(cols):
            f.grid_columnconfigure(k, minsize=250)

    # shop page
    def price_range(self):
        a, b = self.p_min.get().strip(), self.p_max.get().strip()
        if not a and not b:
            return None
        try:
            return (float(a) if a else 0.0, float(b) if b else 1e12)
        except ValueError:
            messagebox.showinfo("Price", "Please type numbers only in the price boxes.")
            return None

    def apply_filters(self, *_):
        key, order = SORTS[self.sort_cb.get()]
        lines = self.run("sort", key, self.alg_var.get(), order)
        items = rows(lines)
        self.info.set(" | ".join(l[1:] for l in lines if l.startswith("#")))
        q = self.search_var.get().strip()
        if q:
            ids = {r["id"] for r in rows(self.run("sname", q.replace(" ", "_")))}
            if not ids:
                ql = q.lower()
                ids = {r["id"] for r in items if ql in (nice(r["name"]) + " " + r["brand"] + " " + r["cat"]).lower()}
            items = [r for r in items if r["id"] in ids]
        pr = self.price_range()
        if pr:
            ids = {r["id"] for r in rows(self.run("sprice", pr[0], pr[1]))}
            items = [r for r in items if r["id"] in ids]
        if self.cat_cb.get() != "All categories":
            items = [r for r in items if r["cat"] == self.cat_cb.get()]
        self.shop.clear()
        txt = "%d products" % len(items) if items else "No products found. Try different words or click Clear all."
        tk.Label(self.shop.inner, text=txt, font=(FONT, 12, "bold"), bg=BG, fg="#374151").pack(anchor="w", padx=20, pady=12)
        self.place_cards(self.shop.inner, [(p, "", "") for p in items])
        self.show_page(self.shop)

    # product page
    def section(self, text, sub=""):
        tk.Label(self.detail.inner, text=text, font=(FONT, 15, "bold"), bg=BG).pack(anchor="w", padx=20, pady=(18, 0))
        if sub:
            tk.Label(self.detail.inner, text=sub, font=(FONT, 10), bg=BG, fg=GREY).pack(anchor="w", padx=20)

    def show_product(self, pid):
        self.current = pid
        p = self.by_id[pid]
        d = self.detail
        d.clear()
        self.show_page(d)
        inner = d.inner
        tk.Button(inner, text="←  Back to all products", relief="flat", bg=BG, fg=NAVY, font=(FONT, 11, "bold"),
                  command=lambda: self.show_page(self.shop)).pack(anchor="w", padx=14, pady=8)
        box = tk.Frame(inner, bg=CARD, highlightbackground="#d1d5db", highlightthickness=1)
        box.pack(fill="x", padx=20)
        tk.Label(box, text=ICON.get(p["cat"], "🛍"), font=(FONT, 70), bg=TINT.get(p["cat"], "#e5e7eb"), width=5).pack(side="left", fill="y")
        info = tk.Frame(box, bg=CARD)
        info.pack(side="left", padx=20, pady=14, anchor="w")
        tk.Label(info, text=nice(p["name"]), font=(FONT, 20, "bold"), bg=CARD).pack(anchor="w")
        tk.Label(info, text="%s  •  %s" % (p["brand"], p["cat"]), font=(FONT, 11), bg=CARD, fg=GREY).pack(anchor="w")
        tk.Label(info, text=stars(p["rating"]), font=(FONT, 13), bg=CARD, fg="#f59e0b").pack(anchor="w", pady=4)
        tk.Label(info, text=money(p["price"]), font=(FONT, 22, "bold"), bg=CARD, fg="#065f46").pack(anchor="w")
        tk.Label(info, text="Features:  " + p["feat"], font=(FONT, 10), bg=CARD, wraplength=600, justify="left").pack(anchor="w", pady=6)
        tk.Button(info, text="🗺  See how products are connected", relief="flat", bg="#dbeafe", fg=NAVY,
                  font=(FONT, 10, "bold"), command=lambda: self.open_map(pid)).pack(anchor="w", pady=4)

        pref = tk.Frame(inner, bg=BG)
        pref.pack(fill="x", padx=20, pady=(14, 0))
        tk.Label(pref, text="Your budget  ₹", bg=BG, font=(FONT, 10)).pack(side="left")
        tk.Entry(pref, textvariable=self.budget_var, width=9).pack(side="left", padx=(2, 14))
        tk.Label(pref, text="Favourite brand", bg=BG, font=(FONT, 10)).pack(side="left")
        tk.Entry(pref, textvariable=self.brand_var, width=12).pack(side="left", padx=(2, 10))
        tk.Button(pref, text="Update suggestions", relief="flat", bg="#e5e7eb",
                  command=lambda: self.show_product(pid)).pack(side="left")

        try:
            budget = float(self.budget_var.get() or 0)
        except ValueError:
            budget = 0
        fav = self.brand_var.get().strip().lower()
        sim = []
        for l in self.run("similar", pid, budget):
            f = l.split("\t")
            if f[0] == "S" and len(f) == 11:
                sim.append({"level": int(f[1]), "match": f[2], "via": f[3], "p": prod(f[4:])})
        sim.sort(key=lambda r: (r["level"], 0 if fav and r["p"]["brand"].lower() == fav else 1))

        def tag(r, text):
            return text + ("   ★ Your favourite brand" if fav and r["p"]["brand"].lower() == fav else "")
        first = [(r["p"], tag(r, "%s%% match" % r["match"]), "#16a34a") for r in sim if r["level"] == 1]
        more = [(r["p"], tag(r, "Goes with %s" % nice(r["via"])), "#6b7280") for r in sim if r["level"] > 1]
        self.section("Customers also like", "The products that are most similar to this one")
        if first:
            self.place_cards(inner, first)
        else:
            tk.Label(inner, text="No similar products found within your budget.", bg=BG, fg=GREY).pack(anchor="w", padx=20, pady=8)
        if more:
            self.section("More you might like", "Other products that go with the ones above")
            self.place_cards(inner, more)

        trail = []
        for l in self.run("trail", pid):
            f = l.split("\t")
            if f[0] == "T" and len(f) == 9:
                trail.append((int(f[1]), prod(f[2:])))
        if trail:
            self.section("Keep exploring", "Follow a path of related products, one after another")
            tf = tk.Frame(inner, bg=BG)
            tf.pack(fill="x", padx=20, pady=(4, 20))
            for depth, q in trail[:12]:
                tk.Button(tf, text="↳  %s   %s" % (nice(q["name"]), money(q["price"])), relief="flat", anchor="w",
                          bg=BG, fg=NAVY, font=(FONT, 10), command=lambda i=q["id"]: self.show_product(i)
                          ).pack(anchor="w", padx=depth * 24)

    # product map
    def open_map(self, pid):
        w = tk.Toplevel(self.root)
        w.title("How products are connected")
        w.geometry("920x700")
        tk.Label(w, text="Products that are alike are joined by a line. Thicker line = more alike.",
                 font=(FONT, 11)).pack(pady=6)
        leg = tk.Frame(w)
        leg.pack()
        for lv, text in [(0, "This product"), (1, "Very similar"), (2, "Similar"), (3, "Loosely related"), (-1, "Not related")]:
            tk.Label(leg, text="●", fg=MAP_COLORS[lv], font=(FONT, 14)).pack(side="left")
            tk.Label(leg, text=text + "   ").pack(side="left")
        c = tk.Canvas(w, bg="white")
        c.pack(fill="both", expand=True, padx=8, pady=8)
        lines = self.run("graph", pid)
        nodes = [l.split("\t")[1:] for l in lines if l.startswith("N\t")]
        edges = [list(map(int, l.split("\t")[1:])) for l in lines if l.startswith("E\t")]
        w.update_idletasks()
        cw, ch = c.winfo_width(), c.winfo_height()
        cx, cy, R = cw / 2, ch / 2, min(cw, ch) / 2 - 85
        pos = []
        for i in range(len(nodes)):
            a = 2 * math.pi * i / len(nodes) - math.pi / 2
            pos.append((cx + R * math.cos(a), cy + R * math.sin(a), a))
        for i, j, wt in edges:
            c.create_line(pos[i][0], pos[i][1], pos[j][0], pos[j][1], fill="#cbd5e1", width=max(1, wt / 40))
        for (x, y, a), (name, lv) in zip(pos, nodes):
            lv = min(int(lv), 3)
            c.create_oval(x - 9, y - 9, x + 9, y + 9, fill=MAP_COLORS[lv], outline="#334155")
            tx, ty = cx + (R + 20) * math.cos(a), cy + (R + 20) * math.sin(a)
            c.create_text(tx, ty, text=nice(name), font=(FONT, 9),
                          anchor="w" if math.cos(a) > 0.1 else ("e" if math.cos(a) < -0.1 else "center"))

    # developer mode (for the teacher demo)
    def open_dev(self):
        if self.dev and self.dev.winfo_exists():
            self.dev.lift()
            return
        w = self.dev = tk.Toplevel(self.root)
        w.title("Developer mode (algorithm details)")
        w.geometry("860x600")
        top = ttk.Frame(w, padding=8)
        top.pack(fill="x")
        ttk.Label(top, text="Product list is sorted using:").grid(row=0, column=0, sticky="w")
        ttk.Radiobutton(top, text="Merge Sort", variable=self.alg_var, value=1, command=self.apply_filters).grid(row=0, column=1)
        ttk.Radiobutton(top, text="Quick Sort", variable=self.alg_var, value=2, command=self.apply_filters).grid(row=0, column=2)
        ttk.Label(top, text="Similarity threshold:").grid(row=1, column=0, sticky="w", pady=4)
        ttk.Spinbox(top, from_=1, to=100, width=5, textvariable=self.threshold,
                    command=self.apply_filters).grid(row=1, column=1, sticky="w")
        ttk.Label(top, text="(two products are linked if similarity is at least this value)").grid(row=1, column=2, columnspan=3, sticky="w")
        ttk.Label(top, textvariable=self.info, foreground="#065f46").grid(row=2, column=0, columnspan=6, sticky="w")
        bar = ttk.Frame(w, padding=8)
        bar.pack(fill="x")
        for text, cmd in [("Speed test", "bench"), ("Complexity table", "complexity"), ("Adjacency list", "net")]:
            ttk.Button(bar, text=text, command=lambda c=cmd: self.dev_show(self.run(c))).pack(side="left", padx=3)
        ttk.Label(bar, text="   Product ID").pack(side="left")
        pid = ttk.Entry(bar, width=5)
        pid.insert(0, str(self.current))
        pid.pack(side="left", padx=3)
        ttk.Button(bar, text="BFS report", command=lambda: self.dev_show(self.run("rec", pid.get() or 1, 0, "-"))).pack(side="left", padx=3)
        ttk.Button(bar, text="DFS report", command=lambda: self.dev_show(self.run("dfs", pid.get() or 1))).pack(side="left", padx=3)
        ttk.Button(bar, text="Add product", command=self.add_dialog).pack(side="left", padx=12)
        fr = ttk.Frame(w)
        fr.pack(fill="both", expand=True, padx=8, pady=6)
        self.dev_text = tk.Text(fr, font=MONO, wrap="none", bg="#fafafa")
        sb = ttk.Scrollbar(fr, command=self.dev_text.yview)
        self.dev_text.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.dev_text.pack(fill="both", expand=True)

    def dev_show(self, lines):
        self.dev_text.delete("1.0", "end")
        self.dev_text.insert("end", "\n".join(l[1:] if l.startswith("#") else l for l in lines))

    def add_dialog(self):
        win = tk.Toplevel(self.root)
        win.title("Add product")
        win.grab_set()
        ents = {}
        for i, lab in enumerate(["Name", "Category", "Brand", "Price", "Rating (0-5)"]):
            ttk.Label(win, text=lab).grid(row=i, column=0, padx=8, pady=3, sticky="w")
            e = ttk.Entry(win, width=24)
            e.grid(row=i, column=1, padx=8)
            ents[lab] = e
        vars_ = []
        for i, f in enumerate(FEATS):
            v = tk.IntVar()
            ttk.Checkbutton(win, text=f, variable=v).grid(row=5 + i // 2, column=i % 2, sticky="w", padx=8)
            vars_.append(v)

        def save():
            try:
                name = ents["Name"].get().strip().replace(" ", "_").replace(",", "")
                cat = ents["Category"].get().strip().replace(",", "")
                brand = ents["Brand"].get().strip().replace(",", "")
                price, rating = float(ents["Price"].get()), float(ents["Rating (0-5)"].get())
                assert name and cat and brand and price > 0 and 0 <= rating <= 5
            except (ValueError, AssertionError):
                messagebox.showerror("Invalid", "Please fill every field with valid values.", parent=win)
                return
            if len(self.by_id) >= 100:
                messagebox.showerror("Limit", "The project supports at most 100 products.", parent=win)
                return
            mask = sum(1 << i for i, v in enumerate(vars_) if v.get())
            with open("products.csv", "a") as f:
                f.write("%s,%s,%s,%g,%.1f,%d\n" % (name, cat, brand, price, rating, mask))
            win.destroy()
            self.reload()

        ttk.Button(win, text="Save", command=save).grid(row=9, column=0, columnspan=2, pady=8)


if __name__ == "__main__":
    build_c_program()
    root = tk.Tk()
    App(root)
    root.mainloop()
