#!/bin/sh
# Builds MovieRotator website.

rm -rf deploy/website/*
mkdir -p deploy/website/image 

for f in website/*.html
do
  # echo $f
  sed -e '/%%DOWNLOAD_BOX%%/ {
    r website/templates/download-box.html
    d
  }' -e '/%%FOOTER%%/ {
    r website/templates/footer.html
    d
  }' -e '/%%FOOTER_ADS%%/ {
    r website/templates/footer_ads.html
    d
  }' -e '/%%SIDE_BAR_ADS%%/ {
    r website/templates/sidebar_ads.html
    d
  }' -e '/%%DONATE_FORM%%/ {
    r website/templates/donate-form.html
    d
  }' -e '/%%DONATE_LINK%%/ {
    r website/templates/donate-link.html
    d
  }' -e '/%%BANNER_AD%%/ {
    r website/templates/banner-ad.html
    d
  }' < $f > deploy/$f
done

cp -r website/image/* deploy/website/image
cp -r website/*.css deploy/website/
cp website/.htaccess deploy/website
cp Installer/MovieRotator*-Setup.exe deploy/website/


