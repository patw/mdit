// AboutDialog — the Help > About box.
//
// A small, themed dialog (not a bare status-bar message, not a plain
// QMessageBox): the brand mark (AppIcons — the white disc + '#'), the name,
// the version, the one-line pitch, the copyright/license line and the **Catbee
// project link**, which is a real hyperlink that opens in the system browser.
//
// The dialog is created once by MainWindow and re-shown (so it never leaks), it
// is window-modal (like every sane About box), it re-centers over the main
// window on each show, and it follows the app theme because it only uses the
// palette/QSS the Theme installs (the muted lines use
// `palette(placeholder-text)` so they read correctly in light *and* dark).
//
// Widgets are private; the accessors exist so the tests can assert the exact
// strings without scraping pixels.
#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QCloseEvent;
class QShowEvent;

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);

    // Center over the parent window, show, raise and focus the Close button.
    void showAbout();

    // -- accessors for tests ------------------------------------------------
    QLabel *iconLabel() const { return m_iconLabel; }
    QLabel *nameLabel() const { return m_nameLabel; }
    QLabel *versionLabel() const { return m_versionLabel; }
    QLabel *taglineLabel() const { return m_taglineLabel; }
    QLabel *licenseLabel() const { return m_licenseLabel; }
    QLabel *projectLabel() const { return m_projectLabel; }
    QPushButton *closeButton() const;

protected:
    // (Re)center over the host window whenever the box is shown.
    void showEvent(QShowEvent *event) override;
    // Re-derive the muted small print + the link color when the theme changes.
    void changeEvent(QEvent *event) override;

private:
    // Center the dialog's frame on parentWidget()->window() (the main window);
    // falls back to the primary screen when there is no visible parent.
    void centerOnParent();
    // (Re)apply the theme-derived colors of the small print + the project link
    // (called on construction and whenever the palette/style changes).
    void applyThemedColors();

    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_versionLabel = nullptr;
    QLabel *m_taglineLabel = nullptr;
    QLabel *m_licenseLabel = nullptr;
    QLabel *m_projectLabel = nullptr;
};
