#ifndef LINEGRAPHWIDGET_H
#define LINEGRAPHWIDGET_H

#include <QWidget>
#include <QColor>
#include <deque>
#include <vector>

class LineGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LineGraphWidget(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setUnitSuffix(const QString &suffix);

    void setMaxSamples(int maxSamples);
    void setFixedRange(double minValue, double maxValue);
    void setAutoScale(bool enabled);

    void addSample(double value);
    void clearSamples();

    double latestValue() const;
    int sampleCount() const;

    void setSeriesNames(const QStringList &names);
    void setSeriesVisible(int index, bool visible);
    bool seriesVisible(int index) const;
    void addSamples(const std::vector<double> &values);
    void clearAllSeries();

    void setShowTitle(bool show);
    void setShowSummaryText(bool show);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

private:
    struct Series
    {
        QString name;
        std::deque<double> samples;
        bool visible = true;
    };

    struct LegendItem
    {
        QRect rect;
        int seriesIndex = -1;
    };

    QString formatValue(double value) const;
    double computedMinValue() const;
    double computedMaxValue() const;
    QPointF graphPoint(int index,
                       double value,
                       const QRectF &plotRect,
                       double minValue,
                       double maxValue,
                       int sampleSlots) const;

    QString latestSummaryText() const;
    void ensureSeriesCount(int count);

    QString m_title;
    QString m_unitSuffix;

    std::deque<double> m_samples;
    int m_maxSamples;

    bool m_autoScale;
    double m_fixedMinValue;
    double m_fixedMaxValue;

    std::vector<Series> m_series;
    std::vector<LegendItem> m_legendItems;

    bool m_showTitle = true;
    bool m_showSummaryText = true;
};

#endif
