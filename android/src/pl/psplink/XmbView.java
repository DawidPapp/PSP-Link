package pl.psplink;

import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.SystemClock;
import android.view.*;
import android.view.animation.DecelerateInterpolator;
import android.widget.*;
import java.util.ArrayList;
import java.util.List;

/** Native accessible controls over a Canvas wave background. */
public final class XmbView extends FrameLayout {
    public interface Actions {
        void configure();
        void refresh();
        void upload();
        void install();
        void download(String name);
    }

    private static final int WHITE = Color.rgb(243, 249, 255);
    private static final int SOFT = Color.rgb(178, 207, 236);
    private static final String[] TITLES = {"Połączenie", "Memory Stick", "Transfery"};
    private final Actions actions;
    private final Wave background;
    private final LinearLayout content, tabs;
    private final ScrollView scroll;
    private final HorizontalScrollView categories;
    private final TextView status, connection;
    private final ProgressBar progress;
    private final ArrayList<String> names = new ArrayList<>();
    private final ArrayList<String> history = new ArrayList<>();
    private final ArrayList<View> commands = new ArrayList<>();
    private int category;
    private boolean busy, loaded;
    private String host = "";

    public XmbView(Context context, Actions actions) {
        super(context);
        this.actions = actions;
        setBackgroundColor(Color.rgb(5, 17, 40));
        background = new Wave(context);
        addView(background, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout page = new LinearLayout(context);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(24), dp(14), dp(24), dp(12));
        addView(page, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout top = new LinearLayout(context);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView logo = label("PSP Link", 21, WHITE);
        logo.setLetterSpacing(.09f);
        top.addView(logo, new LinearLayout.LayoutParams(0, dp(38), 1));
        connection = label("Test połączenia", 12, SOFT);
        top.addView(connection);
        page.addView(top);

        categories = new HorizontalScrollView(context);
        categories.setHorizontalScrollBarEnabled(false);
        categories.setFillViewport(true);
        tabs = new LinearLayout(context);
        tabs.setGravity(Gravity.CENTER_VERTICAL);
        categories.addView(tabs, new HorizontalScrollView.LayoutParams(-1, -1));
        LinearLayout.LayoutParams categoryParams = new LinearLayout.LayoutParams(-1, dp(98));
        categoryParams.topMargin = dp(16);
        page.addView(categories, categoryParams);

        for (int i = 0; i < TITLES.length; ++i) {
            final int index = i;
            LinearLayout tab = new LinearLayout(context);
            tab.setOrientation(LinearLayout.VERTICAL);
            tab.setGravity(Gravity.CENTER);
            tab.setMinimumWidth(dp(102));
            tab.setPadding(dp(8), dp(5), dp(8), dp(9));
            Icon icon = new Icon(context, i);
            tab.addView(icon, new LinearLayout.LayoutParams(dp(34), dp(34)));
            TextView title = label(TITLES[i], 13, WHITE);
            title.setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(-1, dp(32));
            titleParams.topMargin = dp(7);
            tab.addView(title, titleParams);
            tab.setFocusable(true);
            tab.setClickable(true);
            tab.setContentDescription(TITLES[i]);
            tab.setBackground(focusBackground());
            tab.setOnClickListener(v -> select(index, true));
            tabs.addView(tab, new LinearLayout.LayoutParams(0, -1, 1));
        }

        scroll = new ScrollView(context);
        scroll.setFillViewport(true);
        scroll.setVerticalScrollBarEnabled(false);
        content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(0, dp(16), 0, dp(16));
        scroll.addView(content, new ScrollView.LayoutParams(-1, -2));
        page.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));

        progress = new ProgressBar(context, null, android.R.attr.progressBarStyleHorizontal);
        progress.setIndeterminate(true);
        progress.setIndeterminateTintList(android.content.res.ColorStateList.valueOf(SOFT));
        progress.setVisibility(INVISIBLE);
        page.addView(progress, new LinearLayout.LayoutParams(-1, dp(3)));
        status = label("Uruchom PSP Link na konsoli i wpisz adres oraz PIN.", 13, SOFT);
        status.setPadding(0, dp(12), 0, 0);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        page.addView(status, new LinearLayout.LayoutParams(-1, -2));
        select(0, false);
    }

    private int dp(float n) {
        return Math.round(n * getResources().getDisplayMetrics().density);
    }

    private TextView label(String text, float size, int color) {
        TextView view = new TextView(getContext());
        view.setText(text);
        view.setTextSize(size);
        view.setTextColor(color);
        view.setTypeface(Typeface.create("sans-serif-light", Typeface.NORMAL));
        return view;
    }

    private StateListDrawable focusBackground() {
        StateListDrawable drawable = new StateListDrawable();
        GradientDrawable selected = new GradientDrawable(
            GradientDrawable.Orientation.LEFT_RIGHT,
            new int[]{0x254ab6ff, 0x034ab6ff});
        selected.setCornerRadius(dp(10));
        drawable.addState(new int[]{android.R.attr.state_pressed}, selected);
        GradientDrawable focused = new GradientDrawable();
        focused.setColor(0x254ab6ff);
        focused.setCornerRadius(dp(10));
        drawable.addState(new int[]{android.R.attr.state_focused}, focused);
        drawable.addState(new int[]{}, new android.graphics.drawable.ColorDrawable(Color.TRANSPARENT));
        return drawable;
    }

    public int category() { return category; }

    public void select(int index, boolean animate) {
        int direction = Integer.compare(index, category);
        category = Math.max(0, Math.min(2, index));
        for (int i = 0; i < tabs.getChildCount(); ++i) {
            View tab = tabs.getChildAt(i);
            tab.setSelected(i == category);
            tab.animate().alpha(i == category ? 1f : .48f)
                .scaleX(i == category ? 1f : .92f)
                .scaleY(i == category ? 1f : .92f).setDuration(180).start();
        }
        rebuild();
        scroll.scrollTo(0, 0);
        content.animate().cancel();
        content.setAlpha(animate ? 0f : 1f);
        content.setTranslationX(animate ? dp(22) * direction : 0);
        if (animate) {
            content.animate().alpha(1f).translationX(0)
                .setInterpolator(new DecelerateInterpolator()).setDuration(210).start();
        }
        categories.post(() -> {
            View tab = tabs.getChildAt(category);
            categories.smoothScrollTo(tab.getLeft() - (categories.getWidth() - tab.getWidth()) / 2, 0);
        });
    }

    private void item(int icon, String title, String detail, Runnable action, boolean isCommand) {
        LinearLayout row = new LinearLayout(getContext());
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setMinimumHeight(dp(74));
        row.setPadding(dp(8), dp(12), dp(8), dp(12));
        Icon symbol = new Icon(getContext(), icon);
        row.addView(symbol, new LinearLayout.LayoutParams(dp(27), dp(27)));
        LinearLayout text = new LinearLayout(getContext());
        text.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams textParams = new LinearLayout.LayoutParams(0, -2, 1);
        textParams.leftMargin = dp(20);
        row.addView(text, textParams);
        TextView heading = label(title, 19, WHITE);
        heading.setMaxLines(2);
        text.addView(heading);
        if (detail != null && !detail.isEmpty()) {
            TextView sub = label(detail, 13, SOFT);
            sub.setPadding(0, dp(5), 0, 0);
            text.addView(sub);
        }
        if (action != null) {
            row.setBackground(focusBackground());
            row.setFocusable(true);
            row.setClickable(true);
            row.setOnClickListener(v -> { if (!busy || !isCommand) action.run(); });
            row.setContentDescription(title + ". " + (detail == null ? "" : detail));
            row.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
            heading.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            symbol.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
            if (isCommand) {
                commands.add(row);
                row.setEnabled(!busy);
                row.setAlpha(busy ? .45f : 1f);
            }
        }
        content.addView(row, new LinearLayout.LayoutParams(-1, -2));
    }

    private void caption(String text) {
        TextView caption = label(text, 12, SOFT);
        caption.setLetterSpacing(.07f);
        caption.setPadding(dp(55), dp(18), 0, dp(8));
        content.addView(caption);
    }

    private void rebuild() {
        content.removeAllViews();
        commands.clear();
        if (category == 0) {
            item(0, "Dane połączenia", host.isEmpty() ? "Adres IP i PIN z ekranu PSP" : host + " · zmień adres lub PIN",
                 actions::configure, true);
            item(3, "Połącz i odczytaj pliki", "PSP i telefon w tej samej sieci Wi-Fi",
                 actions::refresh, true);
        } else if (category == 1) {
            item(1, "Wgraj aplikację", "EBOOT.PBP → PSP/GAME · do 32 MiB", actions::install, true);
            item(4, "Wyślij plik", "Wybierz dokument .txt z telefonu", actions::upload, true);
            item(3, "Odśwież", "Wczytaj pliki tekstowe w folderze Transfer", actions::refresh, true);
            caption("MEMORY STICK / TRANSFER");
            if (!loaded) {
                item(1, "Odczytaj pliki z PSP", "Wybierz Odśwież po uruchomieniu sesji na konsoli.", null, false);
            } else if (names.isEmpty()) {
                item(1, "Folder jest pusty", "Wyślij pierwszy plik z telefonu.", null, false);
            } else {
                for (String name : names)
                    item(5, name, "Dotknij, aby pobrać na telefon", () -> actions.download(name), true);
            }
        } else {
            if (history.isEmpty()) {
                item(2, "Brak transferów", "Wysłane i pobrane pliki pojawią się tutaj.", null, false);
            } else {
                for (int i = history.size() - 1; i >= 0; --i)
                    item(2, history.get(i), null, null, false);
            }
            caption("WERSJA TESTOWA");
            item(1, "Aplikacje PBP do 32 MiB", "PSP/GAME/Nazwa/EBOOT.PBP · jeden plik", null, false);
            item(5, "Pliki .txt do 1 MiB", "Folder PSP/GAME/PSPLink/Transfer", null, false);
        }
    }

    public void setHost(String value) {
        host = value;
        connection.setText(value.isEmpty() ? "Test połączenia" : value);
        if (category == 0) rebuild();
    }

    public void setBusy(boolean value) {
        busy = value;
        progress.setVisibility(value ? VISIBLE : INVISIBLE);
        for (View command : commands) {
            command.setEnabled(!value);
            command.setAlpha(value ? .45f : 1f);
        }
    }

    public void setStatus(String message) {
        status.setText(message);
    }

    public void setFiles(List<String> files) {
        names.clear();
        names.addAll(files);
        loaded = true;
        if (category == 1) rebuild();
    }

    public void addTransfer(String text) {
        history.add(text);
        if (history.size() > 30) history.remove(0);
        if (category == 2) rebuild();
    }

    public void animateWaves(boolean enabled) {
        background.active = enabled;
        background.invalidate();
    }

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            if (event.getKeyCode() == KeyEvent.KEYCODE_DPAD_LEFT && category > 0) {
                select(category - 1, true);
                return true;
            }
            if (event.getKeyCode() == KeyEvent.KEYCODE_DPAD_RIGHT && category < 2) {
                select(category + 1, true);
                return true;
            }
        }
        return super.dispatchKeyEvent(event);
    }

    private static final class Icon extends View {
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path = new Path();
        private final int kind;

        Icon(Context context, int kind) {
            super(context);
            this.kind = kind;
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        }

        @Override protected void onDraw(Canvas c) {
            c.save();
            c.scale(getWidth() / 32f, getHeight() / 32f);
            p.setColor(WHITE);
            p.setStrokeWidth(2.2f);
            p.setStrokeCap(Paint.Cap.ROUND);
            p.setStrokeJoin(Paint.Join.ROUND);
            p.setStyle(Paint.Style.STROKE);
            switch (kind) {
            case 0:
                c.drawArc(2, 5, 30, 33, 220, 100, false, p);
                c.drawArc(7, 12, 25, 30, 220, 100, false, p);
                c.drawCircle(16, 25, 1.6f, p);
                break;
            case 1:
                path.reset();
                path.moveTo(3, 10); path.lineTo(3, 7); path.lineTo(13, 7);
                path.lineTo(17, 11); path.lineTo(29, 11); path.lineTo(29, 26);
                path.lineTo(3, 26); path.close();
                c.drawPath(path, p);
                break;
            case 2:
                c.drawLine(4, 10, 27, 10, p);
                c.drawLine(22, 5, 27, 10, p); c.drawLine(22, 15, 27, 10, p);
                c.drawLine(28, 23, 5, 23, p);
                c.drawLine(10, 18, 5, 23, p); c.drawLine(10, 28, 5, 23, p);
                break;
            case 3:
                c.drawArc(5, 5, 27, 27, 50, 280, false, p);
                c.drawLine(25, 6, 26, 14, p); c.drawLine(19, 13, 26, 14, p);
                break;
            case 4:
                c.drawLine(16, 5, 16, 22, p);
                c.drawLine(10, 11, 16, 5, p); c.drawLine(22, 11, 16, 5, p);
                c.drawLine(5, 21, 5, 27, p); c.drawLine(5, 27, 27, 27, p);
                c.drawLine(27, 27, 27, 21, p);
                break;
            default:
                path.reset();
                path.moveTo(7, 3); path.lineTo(20, 3); path.lineTo(26, 10);
                path.lineTo(26, 29); path.lineTo(7, 29); path.close();
                path.moveTo(20, 3); path.lineTo(20, 10); path.lineTo(26, 10);
                c.drawPath(path, p);
                c.drawLine(12, 16, 21, 16, p); c.drawLine(12, 21, 21, 21, p);
                break;
            }
            c.restore();
        }
    }

    private static final class Wave extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path ribbon = new Path();
        private Shader gradient;
        private boolean active = true;

        Wave(Context context) {
            super(context);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        }

        @Override protected void onSizeChanged(int w, int h, int oldW, int oldH) {
            gradient = new LinearGradient(0, 0, w * .25f, h,
                new int[]{0xff08152f, 0xff10345a, 0xff186087},
                new float[]{0f, .58f, 1f}, Shader.TileMode.CLAMP);
        }

        private float y(float x, float time, int layer) {
            float u = x / Math.max(1, getWidth());
            return getHeight() * .66f +
                   getHeight() * .055f * (float)Math.sin(u * 4.4f + time + layer * .3f) +
                   getHeight() * .018f * (float)Math.sin(u * 8f - time * .6f);
        }

        @Override protected void onDraw(Canvas canvas) {
            paint.setShader(gradient);
            canvas.drawRect(0, 0, getWidth(), getHeight(), paint);
            paint.setShader(null);
            float time = (SystemClock.uptimeMillis() % 120000) / 12000f;
            for (int layer = 0; layer < 3; ++layer) {
                ribbon.reset();
                ribbon.moveTo(0, y(0, time, layer));
                for (int x = 0; x <= getWidth() + 12; x += 12)
                    ribbon.lineTo(x, y(x, time, layer));
                for (int x = getWidth() + 12; x >= 0; x -= 12)
                    ribbon.lineTo(x, y(x, time, layer) + 12 + layer * 10);
                ribbon.close();
                paint.setColor(layer == 0 ? 0x206dd8ff : 0x126dd8ff);
                canvas.drawPath(ribbon, paint);
            }
            if (active) postInvalidateDelayed(40);
        }
    }
}
