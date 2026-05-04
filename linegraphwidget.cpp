#include "linegraphwidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QMouseEvent>

#include <algorithm>
#include <cmath>

LineGraphWidget::LineGraphWidget(QWidget *parent)
    : QWidget(parent),
      m_maxSamples(60),
      m_autoScale(false),
      m_fixedMinValue(0.0),
      m_fixedMaxValue(100.0)
{
    setMinimumSize(0, 100);
}

void LineGraphWidget::setTitle(const QString &title)
{
    if (m_title == title)
        return;

    m_title = title;
    update();
}

void LineGraphWidget::setUnitSuffix(const QString &suffix)
{
    if (m_unitSuffix == suffix)
        return;

    m_unitSuffix = suffix;
    update();
}

void LineGraphWidget::setMaxSamples(int maxSamples)
{
    if (maxSamples < 2)
        maxSamples = 2;

    m_maxSamples = maxSamples;

    while (static_cast<int>(m_samples.size()) > m_maxSamples)
        m_samples.pop_front();

    for (Series &series : m_series)
    {
        while (static_cast<int>(series.samples.size()) > m_maxSamples)
            series.samples.pop_front();
    }

    update();
}

void LineGraphWidget::setFixedRange(double minValue, double maxValue)
{
    if (maxValue <= minValue)
        maxValue = minValue + 1.0;

    m_fixedMinValue = minValue;
    m_fixedMaxValue = maxValue;
    m_autoScale = false;
    update();
}

void LineGraphWidget::setAutoScale(bool enabled)
{
    m_autoScale = enabled;
    update();
}

void LineGraphWidget::addSample(double value)
{
    m_samples.push_back(value);

    while (static_cast<int>(m_samples.size()) > m_maxSamples)
        m_samples.pop_front();

    update();
}

void LineGraphWidget::clearSamples()
{
    m_samples.clear();
    update();
}

double LineGraphWidget::latestValue() const
{
    if (!m_samples.empty())
        return m_samples.back();

    for (const Series &series : m_series)
    {
        if (!series.samples.empty())
            return series.samples.back();
    }

    return 0.0;
}

int LineGraphWidget::sampleCount() const
{
    if (!m_samples.empty())
        return static_cast<int>(m_samples.size());

    int maxCount = 0;
    for (const Series &series : m_series)
        maxCount = std::max(maxCount, static_cast<int>(series.samples.size()));

    return maxCount;
}

void LineGraphWidget::setSeriesNames(const QStringList &names)
{
    ensureSeriesCount(names.size());

    for (int i = 0; i < names.size(); ++i)
        m_series[i].name = names[i];

    update();
}

void LineGraphWidget::setSeriesVisible(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_series.size()))
        return;

    m_series[index].visible = visible;
    update();
}

void LineGraphWidget::setShowTitle(bool show)
{
    m_showTitle = show;
    update();
}

void LineGraphWidget::setShowSummaryText(bool show)
{
    m_showSummaryText = show;
    update();
}

void LineGraphWidget::setShowAxisLabels(bool show)
{
    m_showAxisLabels = show;
    update();
}

void LineGraphWidget::setSampleIntervalSeconds(int seconds)
{
    if (seconds < 1)
        seconds = 1;

    m_sampleIntervalSeconds = seconds;
    update();
}

bool LineGraphWidget::seriesVisible(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_series.size()))
        return false;

    return m_series[index].visible;
}

void LineGraphWidget::addSamples(const std::vector<double> &values)
{
    ensureSeriesCount(static_cast<int>(values.size()));

    for (int i = 0; i < static_cast<int>(values.size()); ++i)
    {
        m_series[i].samples.push_back(values[i]);

        while (static_cast<int>(m_series[i].samples.size()) > m_maxSamples)
            m_series[i].samples.pop_front();
    }

    update();
}

void LineGraphWidget::clearAllSeries()
{
    for (Series &series : m_series)
        series.samples.clear();

    update();
}

QSize LineGraphWidget::minimumSizeHint() const
{
    return QSize(120, 100);
}

QSize LineGraphWidget::sizeHint() const
{
    return QSize(300, 160);
}

QString LineGraphWidget::formatValue(double value) const
{
    if (m_unitSuffix.contains("B/s"))
    {
        if (std::fabs(value) >= 1024.0 * 1024.0)
            return QString::number(value / (1024.0 * 1024.0), 'f', 1) + " MB/s";

        if (std::fabs(value) >= 1024.0)
            return QString::number(value / 1024.0, 'f', 0) + " KB/s";

        return QString::number(value, 'f', 0) + " B/s";
    }

    if (std::fabs(value) >= 100.0)
        return QString::number(value, 'f', 0) + m_unitSuffix;

    if (std::fabs(value) >= 10.0)
        return QString::number(value, 'f', 1) + m_unitSuffix;

    return QString::number(value, 'f', 2) + m_unitSuffix;
}

QString LineGraphWidget::latestSummaryText() const
{
    if (!m_series.empty())
    {
        QStringList parts;

        for (int i = 0; i < static_cast<int>(m_series.size()); ++i)
        {
            const Series &series = m_series[i];
            if (!series.visible || series.samples.empty())
                continue;

            double value = series.samples.back();
            QString text = formatValue(value);

            if (!series.name.isEmpty())
                text = series.name + ": " + text;

            parts << text;
        }

        return parts.join("  ");
    }

    if (!m_samples.empty())
        return formatValue(m_samples.back());

    return "";
}

double LineGraphWidget::computedMinValue() const
{
    if (!m_autoScale)
        return m_fixedMinValue;

    bool found = false;
    double minValue = 0.0;

    if (!m_samples.empty())
    {
        auto it = std::min_element(m_samples.begin(), m_samples.end());
        minValue = *it;
        found = true;
    }

    for (const Series &series : m_series)
    {
        if (!series.visible || series.samples.empty())
            continue;

        auto it = std::min_element(series.samples.begin(), series.samples.end());
        if (!found || *it < minValue)
        {
            minValue = *it;
            found = true;
        }
    }

    if (!found)
        return m_fixedMinValue;

    double maxValue = computedMaxValue();
    if (std::fabs(maxValue - minValue) < 1e-9)
        return minValue - 1.0;

    double padding = (maxValue - minValue) * 0.10;
    return minValue - padding;
}

double LineGraphWidget::computedMaxValue() const
{
    if (!m_autoScale)
        return m_fixedMaxValue;

    bool found = false;
    double maxValue = 0.0;

    if (!m_samples.empty())
    {
        auto it = std::max_element(m_samples.begin(), m_samples.end());
        maxValue = *it;
        found = true;
    }

    for (const Series &series : m_series)
    {
        if (!series.visible || series.samples.empty())
            continue;

        auto it = std::max_element(series.samples.begin(), series.samples.end());
        if (!found || *it > maxValue)
        {
            maxValue = *it;
            found = true;
        }
    }

    if (!found)
        return m_fixedMaxValue;

    double minValue = maxValue;
    bool minFound = false;

    if (!m_samples.empty())
    {
        auto it = std::min_element(m_samples.begin(), m_samples.end());
        minValue = *it;
        minFound = true;
    }

    for (const Series &series : m_series)
    {
        if (!series.visible || series.samples.empty())
            continue;

        auto it = std::min_element(series.samples.begin(), series.samples.end());
        if (!minFound || *it < minValue)
        {
            minValue = *it;
            minFound = true;
        }
    }

    if (std::fabs(maxValue - minValue) < 1e-9)
        return maxValue + 1.0;

    double padding = (maxValue - minValue) * 0.10;
    return maxValue + padding;
}

QPointF LineGraphWidget::graphPoint(int index,
                                    double value,
                                    const QRectF &plotRect,
                                    double minValue,
                                    double maxValue,
                                    int sampleSlots) const
{
    double x = plotRect.left();
    if (sampleSlots > 1)
    {
        x += (plotRect.width() * static_cast<double>(index)) /
             static_cast<double>(sampleSlots - 1);
    }

    double range = maxValue - minValue;
    if (range <= 0.0)
        range = 1.0;

    double normalized = (value - minValue) / range;
    normalized = std::clamp(normalized, 0.0, 1.0);

    double y = plotRect.bottom() - normalized * plotRect.height();
    return QPointF(x, y);
}

void LineGraphWidget::ensureSeriesCount(int count)
{
    if (count < 0)
        count = 0;

    while (static_cast<int>(m_series.size()) < count)
        m_series.push_back(Series{});

    while (static_cast<int>(m_series.size()) > count)
        m_series.pop_back();
}

void LineGraphWidget::mousePressEvent(QMouseEvent *event)
{
    for (const LegendItem &item : m_legendItems)
    {
        if (item.rect.contains(event->pos()))
        {
            if (item.seriesIndex >= 0 && item.seriesIndex < static_cast<int>(m_series.size()))
            {
                m_series[item.seriesIndex].visible = !m_series[item.seriesIndex].visible;
                update();
                return;
            }
        }
    }

    QWidget::mousePressEvent(event);
}

void LineGraphWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    m_legendItems.clear();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().window());

    QRectF outer = rect().adjusted(16, 12, -16, -14);

    bool hasHeader = m_showTitle || m_showSummaryText;
    QRectF titleRect = outer.adjusted(8, 6, -8, 0);

    if (m_showTitle)
    {
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize());
        painter.setFont(titleFont);
        painter.setPen(palette().text().color());
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, m_title);
    }

    if (m_showSummaryText)
    {
        QFont valueFont = painter.font();
        valueFont.setBold(false);
        painter.setFont(valueFont);
        painter.setPen(palette().text().color());

        QRectF summaryRect = outer.adjusted(8, 10, -12, 0);
        painter.drawText(summaryRect, Qt::AlignRight | Qt::AlignTop, latestSummaryText());
    }

    bool hasLegend = !m_series.empty();
    int topInset = hasHeader ? 28 : 8;
    if (hasLegend)
    topInset += 24;

    QRectF plotRect;
    int labelWidth = 0;

    if (m_showAxisLabels)
    {
        QFontMetrics fm(painter.font());
        labelWidth = fm.horizontalAdvance("100 MB/s") + 8;

        plotRect = outer.adjusted(labelWidth + 10, topInset, -6, -18);
    }
    else
    {
        plotRect = outer.adjusted(8, topInset, -8, -8);
    }

    double minValue = computedMinValue();
    double maxValue = computedMaxValue();
    if (maxValue <= minValue)
        maxValue = minValue + 1.0;

    painter.setPen(QPen(palette().midlight().color(), 1, Qt::DashLine));
    const int gridLines = 4;
    for (int i = 0; i <= gridLines; ++i)
    {
        double y = plotRect.top() + (plotRect.height() * i) / gridLines;
        painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
    }

    for (int i = 0; i <= 5; ++i)
    {
        double x = plotRect.left() + (plotRect.width() * i) / 5.0;
        painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
    }

    // Legend and line drawing

    if (m_showAxisLabels)
    {
        painter.setPen(QPen(palette().mid().color(), 1));

        painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());
        painter.drawLine(plotRect.bottomLeft(), plotRect.topLeft());

        QFont axisFont = painter.font();
        axisFont.setPointSize(std::max(7, axisFont.pointSize() - 2));
        painter.setFont(axisFont);
        painter.setPen(palette().text().color());

        const int yTicks = 2;
        for (int i = 0; i <= yTicks; ++i)
        {
            double ratio = static_cast<double>(i) / yTicks;
            double value = maxValue - ratio * (maxValue - minValue);
            double y = plotRect.top() + plotRect.height() * ratio;

            painter.drawLine(QPointF(plotRect.left() - 4, y), QPointF(plotRect.left(), y));

            QString label = formatValue(value);
            QRectF labelRect(plotRect.left() - labelWidth - 6, y - 8, labelWidth, 16);
            painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, label);
        }

        const int xTicks = 2;
        int totalSeconds = std::max(1, (m_maxSamples - 1) * m_sampleIntervalSeconds);

        for (int i = 0; i <= xTicks; ++i)
        {
            double ratio = static_cast<double>(i) / xTicks;
            double x = plotRect.left() + plotRect.width() * ratio;

            painter.drawLine(QPointF(x, plotRect.bottom()), QPointF(x, plotRect.bottom() + 6));

            int secondsAgo = static_cast<int>((1.0 - ratio) * totalSeconds);
            QString label = (i == xTicks) ? "now" : QString("-%1s").arg(secondsAgo);

            QRectF labelRect(x - 18, plotRect.bottom() + 7, 36, 14);
            painter.drawText(labelRect, Qt::AlignCenter, label);
        }
    }
    if (!m_series.empty())
    {
        const std::vector<QColor> lineColors = {
            QColor(37, 99, 235), QColor(22, 163, 74), QColor(220, 38, 38),
            QColor(168, 85, 247), QColor(234, 88, 12)
        };

        int legendY = static_cast<int>(outer.top()) + 10;
        int legendX = static_cast<int>(outer.left()) + 8;

        for (int s = 0; s < static_cast<int>(m_series.size()); ++s)
        {
            const Series &series = m_series[s];
            QColor color = lineColors[s % lineColors.size()];
            QRect boxRect(legendX, legendY, 12, 12);
            painter.setBrush(series.visible ? color : palette().mid().color());
            painter.setPen(QPen(palette().dark().color(), 1));
            painter.drawRect(boxRect);

            QRect clickRect(legendX, legendY - 2, 80, 18);
            m_legendItems.push_back({clickRect, s});

            painter.setPen(palette().text().color());
            QString name = series.name.isEmpty() ? QString("Series %1").arg(s + 1) : series.name;
            painter.drawText(QRectF(legendX + 16, legendY - 3, 80, 18),
                             Qt::AlignLeft | Qt::AlignVCenter, name);
            legendX += 90;
        }

        bool hasData = false;
        for (const Series &series : m_series)
        {
            if (!series.visible || series.samples.empty())
                continue;

            hasData = true;
            break;
        }

        if (!hasData)
        {
            painter.setPen(palette().mid().color());
            painter.drawText(plotRect, Qt::AlignCenter, "No data");
            return;
        }

        for (int s = 0; s < static_cast<int>(m_series.size()); ++s)
        {
            const Series &series = m_series[s];
            if (!series.visible || series.samples.empty())
                continue;

            QColor color = lineColors[s % lineColors.size()];
            QPainterPath path;
            bool started = false;
            int sampleSlots = std::max(2, static_cast<int>(series.samples.size()));
            int offset = 0;

            for (int i = 0; i < static_cast<int>(series.samples.size()); ++i)
            {
                int graphIndex = offset + i;
                QPointF pt = graphPoint(graphIndex, series.samples[i], plotRect, minValue, maxValue, sampleSlots);
                if (!started)
                {
                    path.moveTo(pt);
                    started = true;
                }
                else
                {
                    path.lineTo(pt);
                }
            }

            painter.setPen(QPen(color, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);

            QPointF lastPoint = graphPoint(offset + static_cast<int>(series.samples.size()) - 1,
                                           series.samples.back(), plotRect, minValue, maxValue, sampleSlots);
            painter.setBrush(color);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(lastPoint, 4, 4);
        }
    }
    else
    {
        bool hasData = !m_samples.empty();
        if (!hasData)
        {
            painter.setPen(palette().mid().color());
            painter.drawText(plotRect, Qt::AlignCenter, "No data");
            return;
        }

        QPainterPath path;
        bool started = false;
        int sampleSlots = std::max(2, static_cast<int>(m_samples.size()));
        int offset = 0;

        for (int i = 0; i < static_cast<int>(m_samples.size()); ++i)
        {
            int graphIndex = offset + i;
            QPointF pt = graphPoint(graphIndex, m_samples[i], plotRect, minValue, maxValue, sampleSlots);
            if (!started)
            {
                path.moveTo(pt);
                started = true;
            }
            else
            {
                path.lineTo(pt);
            }
        }

        QPen linePen(palette().highlight().color(), 2);
        painter.setPen(linePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        QPointF lastPoint = graphPoint(offset + static_cast<int>(m_samples.size()) - 1,
                                       m_samples.back(), plotRect, minValue, maxValue, sampleSlots);
        painter.setBrush(palette().highlight());
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(lastPoint, 4, 4);
    }
}
