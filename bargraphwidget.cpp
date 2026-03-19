#include "bargraphwidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <algorithm>
#include <cmath>

BarGraphWidget::BarGraphWidget(QWidget *parent)
    : QWidget(parent),
    m_minValue(0.0),
    m_maxValue(100.0),
    m_singleValue(0.0),
    m_hasSingleValue(false)
{
    setMinimumHeight(120);
}

void BarGraphWidget::setTitle(const QString &title)
{
    if (m_title == title)
        return;

    m_title = title;
    update();
}

void BarGraphWidget::setUnitSuffix(const QString &suffix)
{
    if (m_unitSuffix == suffix)
        return;

    m_unitSuffix = suffix;
    update();
}

void BarGraphWidget::setRange(double minValue, double maxValue)
{
    if (maxValue <= minValue)
        maxValue = minValue + 1.0;

    m_minValue = minValue;
    m_maxValue = maxValue;
    update();
}

void BarGraphWidget::setValue(double value)
{
    m_singleValue = value;
    m_hasSingleValue = true;
    m_labels.clear();
    m_values.clear();
    update();
}

void BarGraphWidget::setBars(const std::vector<QString> &labels,
                             const std::vector<double> &values)
{
    m_labels = labels;
    m_values = values;
    m_hasSingleValue = false;
    update();
}

void BarGraphWidget::clearBars()
{
    m_labels.clear();
    m_values.clear();
    m_hasSingleValue = false;
    update();
}

QSize BarGraphWidget::minimumSizeHint() const
{
    return QSize(240, 90);
}

QSize BarGraphWidget::sizeHint() const
{
    return QSize(360, 110);
}

QString BarGraphWidget::formatValue(double value) const
{
    if (std::fabs(value) >= 100.0)
        return QString::number(value, 'f', 0) + m_unitSuffix;

    if (std::fabs(value) >= 10.0)
        return QString::number(value, 'f', 1) + m_unitSuffix;

    return QString::number(value, 'f', 2) + m_unitSuffix;
}

void BarGraphWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), palette().window());

    QRectF outer = rect().adjusted(8, 4, -8, -4);

    painter.setPen(QPen(palette().mid().color(), 1));
    painter.drawRoundedRect(outer, 8, 8);

    QFont valueFont = painter.font();
    valueFont.setBold(false);
    painter.setFont(valueFont);

    if (m_hasSingleValue)
    {
        double range = m_maxValue - m_minValue;
        if (range <= 0.0)
            range = 1.0;

        double clampedValue = std::clamp(m_singleValue, m_minValue, m_maxValue);
        double normalized = (clampedValue - m_minValue) / range;

        QRectF contentRect = outer.adjusted(14, 8, -14, -8);

        const double valueWidth = 90.0;
        const double gap = 12.0;

        QRectF barBg(contentRect.left(),
                     contentRect.center().y() - 16,
                     contentRect.width() - valueWidth - gap,
                     32);

        QRectF valueRect(barBg.right() + gap,
                         contentRect.top(),
                         valueWidth,
                         contentRect.height());

        QRectF barFill(barBg.left(),
                       barBg.top(),
                       barBg.width() * normalized,
                       barBg.height());

        painter.setPen(QPen(palette().mid().color(), 1));
        painter.setBrush(QColor(243, 243, 243));
        painter.drawRoundedRect(barBg, 6, 6);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(249, 115, 22));
        painter.drawRoundedRect(barFill, 6, 6);

        painter.setPen(palette().text().color());
        painter.drawText(valueRect,
                         Qt::AlignRight | Qt::AlignVCenter,
                         formatValue(m_singleValue));

        return;
    }

    if (m_values.empty())
    {
        painter.setPen(palette().mid().color());
        painter.drawText(outer.adjusted(12, 8, -12, -8), Qt::AlignCenter, "No data");
        return;
    }

    QRectF contentRect = outer.adjusted(14, 10, -14, -10);

    int n = static_cast<int>(m_values.size());
    if (n <= 0)
        return;

    double rowHeight = contentRect.height() / std::max(1, n);
    double labelWidth = contentRect.width() * 0.28;
    double valueWidth = contentRect.width() * 0.18;
    double barWidth = contentRect.width() - labelWidth - valueWidth - 16.0;

    for (int i = 0; i < n; ++i)
    {
        double y = contentRect.top() + i * rowHeight;

        QRectF labelRect(contentRect.left(), y, labelWidth, rowHeight);
        QRectF barRect(contentRect.left() + labelWidth + 8, y + 6, barWidth, rowHeight - 12);
        QRectF valueRect(barRect.right() + 8, y, valueWidth, rowHeight);

        QString label = (i < static_cast<int>(m_labels.size())) ? m_labels[i] : QString("Item %1").arg(i + 1);
        double value = m_values[i];

        double range = m_maxValue - m_minValue;
        if (range <= 0.0)
            range = 1.0;

        double clampedValue = std::clamp(value, m_minValue, m_maxValue);
        double normalized = (clampedValue - m_minValue) / range;

        QRectF fillRect(barRect.left(), barRect.top(), barRect.width() * normalized, barRect.height());

        painter.setPen(palette().text().color());
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, label);

        painter.setPen(QPen(palette().mid().color(), 1));
        painter.setBrush(QColor(243, 243, 243));
        painter.drawRoundedRect(barRect, 4, 4);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(249, 115, 22));
        painter.drawRoundedRect(fillRect, 4, 4);

        painter.setPen(palette().text().color());
        painter.drawText(valueRect, Qt::AlignRight | Qt::AlignVCenter, formatValue(value));
    }
}
