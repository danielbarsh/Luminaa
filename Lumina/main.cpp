#include <SFML/Graphics.hpp>

void update(sf::RenderWindow& window);
void draw(sf::RenderWindow& window);


int main()
{
	sf::RenderWindow window(sf::VideoMode({ 800, 600 }), "RGP game");

	while (window.isOpen()) {
		
		update(window);
	    draw(window);

	}


	return 0;
}

void update(sf::RenderWindow& window) {

	while (const std::optional event = window.pollEvent()) {

		if (event->is<sf::Event::Closed>()) {
			window.close();
		}
 
	}
}

void draw(sf::RenderWindow& window) {

	window.clear(sf::Color::Red);

	window.display();

}