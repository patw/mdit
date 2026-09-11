#include "AboutDialog.h"

#include "AppIcons.h"
#include "AppInfo.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {

// A muted secondary line (version / license / project): the window-text color at
// 65% alpha, so it is legible in BOTH themes. (`palette(placeholder-text)`
// looked fine in light mode but is unset in the dark palette, which left the
// small print nearly invisible — hence the computed color.)
QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

// "rgba(r, g, b, 65%)" for the palette's window-text color.
QString mutedColorDeclaration(const QWidget &w)
{
    const QColor c = w.palette().color(QPalette::WindowText);
    return QStringLiteral("color: rgba(%1, %2, %3, 65%);")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue());
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("AboutDialog"));
    setWindowTitle(tr("About mdit"));
    setWindowIcon(AppIcons::windowIcon());
    // A tidy, fixed-feeling box: window-modal like a normal About, not an
    // input-blocking app-modal one, and no context-help '?' in the title bar.
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(420);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(20, 18, 20, 14);
    outer->setSpacing(12);

    // --- top: the mark on the left, name/version/tagline on the right. -------
    auto *top = new QHBoxLayout;
    top->setSpacing(18);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setObjectName(QStringLiteral("AboutIcon"));
    const QPixmap mark = AppIcons::windowIcon().pixmap(96, 96);
    m_iconLabel->setPixmap(mark);
    m_iconLabel->setFixedSize(96, 96);
    top->addWidget(m_iconLabel, 0, Qt::AlignTop);

    auto *headline = new QVBoxLayout;
    headline->setSpacing(4);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    m_nameLabel = new QLabel(AppInfo::name(), this);
    m_nameLabel->setObjectName(QStringLiteral("AboutName"));
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSizeF(nameFont.pointSizeF() * 1.9);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    titleRow->addWidget(m_nameLabel, 0, Qt::AlignBottom);

    m_versionLabel = mutedLabel(tr("version %1").arg(AppInfo::version()), this);
    m_versionLabel->setObjectName(QStringLiteral("AboutVersion"));
    titleRow->addWidget(m_versionLabel, 0, Qt::AlignBottom);
    titleRow->addStretch(1);
    headline->addLayout(titleRow);

    m_taglineLabel = new QLabel(AppInfo::tagline(), this);
    m_taglineLabel->setObjectName(QStringLiteral("AboutTagline"));
    m_taglineLabel->setWordWrap(true);
    headline->addWidget(m_taglineLabel);
    headline->addStretch(1);

    top->addLayout(headline, 1);
    outer->addLayout(top);

    // --- separator + the small print. ----------------------------------------
    auto *rule = new QFrame(this);
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Sunken);
    outer->addWidget(rule);

    m_licenseLabel = mutedLabel(AppInfo::copyrightLine(), this);
    m_licenseLabel->setObjectName(QStringLiteral("AboutLicense"));
    outer->addWidget(m_licenseLabel);

    // The Catbee line, with the URL as a real link (opens in the browser). The
    // link color is the palette's highlight (a strong blue in both themes —
    // Qt's default link color is a dark navy that vanishes on the dark
    // background); applyThemedColors() keeps it right across theme switches.
    m_projectLabel = mutedLabel(QString(), this);
    m_projectLabel->setObjectName(QStringLiteral("AboutProject"));
    m_projectLabel->setTextFormat(Qt::RichText);
    m_projectLabel->setOpenExternalLinks(true);
    outer->addWidget(m_projectLabel);

    // --- the Close button, bottom right. -------------------------------------
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setObjectName(QStringLiteral("AboutButtons"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    outer->addWidget(buttons);

    applyThemedColors();
}

void AboutDialog::applyThemedColors()
{
    // Muted small print (theme-aware: window text at 65%).
    const QString muted = mutedColorDeclaration(*this);
    for (QLabel *label : {m_versionLabel, m_licenseLabel, m_projectLabel}) {
        if (label)
            label->setStyleSheet(muted);
    }

    // The project line's link, in the theme's highlight blue.
    if (m_projectLabel) {
        m_projectLabel->setText(
            QStringLiteral("%1 &mdash; <a href=\"%2\" style=\"color:%3;"
                           " text-decoration:underline;\">%4</a>")
                .arg(AppInfo::projectLine().toHtmlEscaped(),
                     AppInfo::projectUrl(),
                     palette().color(QPalette::Highlight).name(),
                     AppInfo::projectUrl().remove(QStringLiteral("https://"))));
    }
}

void AboutDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    // A theme toggle (or a style change) re-colors the muted lines + the link.
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::StyleChange
        || event->type() == QEvent::ApplicationPaletteChange) {
        applyThemedColors();
    }
}

QPushButton *AboutDialog::closeButton() const
{
    const auto *box = findChild<QDialogButtonBox *>(QStringLiteral("AboutButtons"));
    return box ? box->button(QDialogButtonBox::Close) : nullptr;
}

void AboutDialog::showAbout()
{
    centerOnParent();
    show();
    raise();
    activateWindow();
    if (QPushButton *close = closeButton())
        close->setFocus(Qt::OtherFocusReason);
}

void AboutDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    centerOnParent(); // re-center on every show (the size is fixed per theme)
}

void AboutDialog::centerOnParent()
{
    const QSize sz = frameGeometry().size();
    const QPoint offset(sz.width() / 2, sz.height() / 2);
    QWidget *host = parentWidget() ? parentWidget()->window() : nullptr;
    if (host && host->isVisible()) {
        move(host->frameGeometry().center() - offset);
        return;
    }
    if (const QScreen *screen = QGuiApplication::primaryScreen())
        move(screen->availableGeometry().center() - offset);
}
