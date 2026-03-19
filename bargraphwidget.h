#ifndef BARGRAPHWIDGET_H
#define BARGRAPHWIDGET_H

#include <QWidget>
#include <QString>
#include <vector>

class BarGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BarGraphWidget(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setUnitSuffix(const QString &suffix);

    void setRange(double minValue, double maxValue);
    void setValue(double value);

    void setBars(const std::vector<QString> &labels,
                 const std::vector<double> &values);

    void clearBars();

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

private:
    QString formatValue(double value) const;

    QString m_title;
    QString m_unitSuffix;

    double m_minValue;
    double m_maxValue;
    double m_singleValue;
    bool m_hasSingleValue;

    std::vector<QString> m_labels;
    std::vector<double> m_values;
};

#endif
